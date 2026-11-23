// The plain looks: the CPU's name, memory, drives, graphics adapters
// (decision 173). Every API here is Windows 7 or older, so none needs
// a runtime lookup (decision 015).
#include "providers/statistics/stats_sources.h"

#include "platform/win.h"

#include <dxgi.h>
#include <psapi.h>

#include <algorithm>
#include <cwctype>

namespace cup::stats {
namespace {

std::uint64_t ticks(const FILETIME& time) // 100 ns
{
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

std::string trimmed(std::string text)
{
    while (!text.empty() && text.back() == ' ') text.pop_back();
    text.erase(0, text.find_first_not_of(' ') == std::string::npos ? text.size() : text.find_first_not_of(' '));
    return text;
}

} // namespace

std::string utf8(const wchar_t* text)
{
    if (!text || !*text) return {};
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr); // with the ending zero
    if (length <= 1) return {};
    std::string out(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), length, nullptr, nullptr);
    out.resize(static_cast<std::size_t>(length) - 1);
    return out;
}

std::string cpuName()
{
    wchar_t name[256] = {};
    DWORD bytes = sizeof(name) - sizeof(wchar_t);
    LSTATUS status = RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString", RRF_RT_REG_SZ,
                                  nullptr, name, &bytes);
    return status == ERROR_SUCCESS ? trimmed(utf8(name)) : std::string{}; // Intel pads its names with spaces
}

std::size_t coreCount()
{
    DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return count > 0 ? count : 1;
}

std::optional<CpuTimes> cpuTimes()
{
    FILETIME idle{}, kernel{}, user{};
    if (!GetSystemTimes(&idle, &kernel, &user)) return std::nullopt;
    return CpuTimes{ ticks(idle), ticks(kernel), ticks(user) };
}

std::optional<Memory> memory()
{
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status) || status.ullTotalPhys == 0) return std::nullopt;
    // In use is what isn't available; available is standby and free together.
    Memory m{ .used = status.ullTotalPhys - std::min(status.ullAvailPhys, status.ullTotalPhys), .total = status.ullTotalPhys };
    PERFORMANCE_INFORMATION performance{};
    performance.cb = sizeof(performance);
    if (GetPerformanceInfo(&performance, sizeof(performance)))
        m.cache = std::min<std::uint64_t>(static_cast<std::uint64_t>(performance.SystemCache) * performance.PageSize, status.ullAvailPhys);
    return m;
}

std::vector<DriveReading> drives()
{
    wchar_t windows[MAX_PATH] = {};
    wchar_t systemLetter = GetWindowsDirectoryW(windows, MAX_PATH) > 0 ? windows[0] : L'C';
    std::vector<DriveReading> found;
    DWORD letters = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(letters & (1u << i))) continue;
        wchar_t letter = static_cast<wchar_t>(L'A' + i);
        const wchar_t root[] = { letter, L':', L'\\', 0 };
        if (GetDriveTypeW(root) != DRIVE_FIXED) continue; // never a network drive: asking a dead one hangs
        wchar_t label[MAX_PATH + 1] = {};
        GetVolumeInformationW(root, label, MAX_PATH + 1, nullptr, nullptr, nullptr, nullptr, 0); // a locked drive: no label
        std::string name = utf8(label);
        const wchar_t shortRoot[] = { letter, L':', 0 };
        DriveReading drive{ .label = (name.empty() ? std::string("Local Disk") : name) + " (" + utf8(shortRoot) + ")" };
        ULARGE_INTEGER freeForUs{}, total{}, free{};
        if (GetDiskFreeSpaceExW(root, &freeForUs, &total, &free) && total.QuadPart >= free.QuadPart) {
            drive.used = total.QuadPart - free.QuadPart;
            drive.total = total.QuadPart;
        }
        bool isSystem = letter == static_cast<wchar_t>(std::towupper(systemLetter));
        if (isSystem) found.insert(found.begin(), std::move(drive));
        else found.push_back(std::move(drive));
    }
    return found;
}

std::vector<GpuAdapter> gpuAdapters()
{
    std::vector<GpuAdapter> found;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) || !factory) return found;
    for (UINT i = 0;; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) != S_OK || !adapter) break; // DXGI_ERROR_NOT_FOUND: no more
        DXGI_ADAPTER_DESC1 description{};
        if (SUCCEEDED(adapter->GetDesc1(&description))) {
            found.push_back(GpuAdapter{
                .luid = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(description.AdapterLuid.HighPart)) << 32) | description.AdapterLuid.LowPart,
                .name = utf8(description.Description),
                .dedicatedMemory = description.DedicatedVideoMemory,
                .software = (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0,
            });
        }
        adapter->Release();
    }
    factory->Release();
    return found;
}

} // namespace cup::stats
