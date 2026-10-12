#include "platform/displays.h"

#include <shellscalingapi.h>

#include <map>

namespace cup::platform {
namespace {

std::string narrow(const wchar_t* text)
{
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return {};
    std::string out(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), length, nullptr, nullptr);
    return out;
}

struct Identity {
    std::string id;
    std::string name;
};

// GDI device name ("\\.\DISPLAY2", which can change between boots) -> the
// monitor's stable device path and friendly name, via the display config API.
std::map<std::wstring, Identity> monitorIdentities()
{
    std::map<std::wstring, Identity> identities;
    UINT32 pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) return identities;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS)
        return identities;

    for (UINT32 i = 0; i < pathCount; ++i) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size = sizeof source;
        source.header.adapterId = paths[i].sourceInfo.adapterId;
        source.header.id = paths[i].sourceInfo.id;
        DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
        target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        target.header.size = sizeof target;
        target.header.adapterId = paths[i].targetInfo.adapterId;
        target.header.id = paths[i].targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
            DisplayConfigGetDeviceInfo(&target.header) != ERROR_SUCCESS)
            continue;
        identities[source.viewGdiDeviceName] = { narrow(target.monitorDevicePath), narrow(target.monitorFriendlyDeviceName) };
    }
    return identities;
}

BOOL CALLBACK addMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM out)
{
    auto& displays = *reinterpret_cast<std::vector<std::pair<HMONITOR, MONITORINFOEXW>>*>(out);
    MONITORINFOEXW info{};
    info.cbSize = sizeof info;
    if (GetMonitorInfoW(monitor, &info)) displays.emplace_back(monitor, info);
    return TRUE;
}

} // namespace

std::vector<Display> listDisplays()
{
    std::vector<std::pair<HMONITOR, MONITORINFOEXW>> monitors;
    EnumDisplayMonitors(nullptr, nullptr, addMonitor, reinterpret_cast<LPARAM>(&monitors));
    auto identities = monitorIdentities();

    std::vector<Display> displays;
    std::map<std::string, int> sameName;
    for (const auto& [monitor, info] : monitors) {
        Display d;
        auto known = identities.find(info.szDevice);
        if (known != identities.end() && !known->second.id.empty()) d.id = known->second.id;
        else d.id = narrow(info.szDevice); // no stable path (rare: remote sessions, some virtual displays)
        d.name = known != identities.end() && !known->second.name.empty()
                     ? known->second.name
                     : "Display " + std::to_string(displays.size() + 1);
        if (int n = ++sameName[d.name]; n > 1) d.name += " (" + std::to_string(n) + ")";
        d.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
        const RECT& r = info.rcMonitor;
        d.bounds = { static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right - r.left),
                     static_cast<float>(r.bottom - r.top) };
        UINT dpiX = 96, dpiY = 96;
        if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) d.scale = static_cast<float>(dpiX) / 96.0f;
        displays.push_back(std::move(d));
    }
    return displays;
}

Display displayFor(const std::string& id)
{
    auto displays = listDisplays();
    for (const auto& d : displays)
        if (!id.empty() && d.id == id) return d;
    for (const auto& d : displays)
        if (d.primary) return d;
    if (!displays.empty()) return displays.front();
    // No display reported at all: assume a 100% primary at the origin.
    return { .id = "", .name = "Display", .primary = true,
             .bounds = { 0, 0, static_cast<float>(GetSystemMetrics(SM_CXSCREEN)), static_cast<float>(GetSystemMetrics(SM_CYSCREEN)) } };
}

} // namespace cup::platform
