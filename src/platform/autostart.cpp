#include "platform/autostart.h"

#include "platform/win.h"

#include <string>

namespace cup::platform {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"Cup Island"; // installer/cup-island.iss removes it on uninstall

// Quoted, because the install path usually contains spaces.
std::wstring command()
{
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    return L"\"" + std::wstring(exePath) + L"\"";
}

} // namespace

bool startsWithWindows()
{
    wchar_t value[MAX_PATH + 3]{};
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS) return false;
    return _wcsicmp(value, command().c_str()) == 0;
}

void setStartsWithWindows(bool on)
{
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    if (on) {
        std::wstring value = command();
        RegSetValueExW(key, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kValueName);
    }
    RegCloseKey(key);
}

} // namespace cup::platform
