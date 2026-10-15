#include "platform/window_name.h"

namespace cup::platform {

std::wstring describeWindow(HWND hwnd)
{
    wchar_t cls[128]{}, title[128]{};
    GetClassNameW(hwnd, cls, 128);
    GetWindowTextW(hwnd, title, 128);
    std::wstring exe = L"?";
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t path[MAX_PATH]{};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, path, &size)) exe = std::wstring(path).substr(std::wstring(path).find_last_of(L'\\') + 1);
        CloseHandle(process);
    }
    return exe + L" (class " + cls + L", \"" + title + L"\")";
}

} // namespace cup::platform
