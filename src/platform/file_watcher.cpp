#include "platform/file_watcher.h"
#include "platform/log.h"

#include "platform/win.h"

#include <cstdio>
#include <thread>

namespace cup::platform {

void watchDirectoryOf(const std::filesystem::path& file, std::function<void()> onChange)
{
    std::filesystem::path directory = file.parent_path();
    HANDLE change = FindFirstChangeNotificationW(directory.c_str(), /* watchSubtree */ FALSE,
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
    if (change == INVALID_HANDLE_VALUE) {
        logLine("can't watch %ls (0x%08lx); live reload is off", directory.c_str(), GetLastError());
        return;
    }

    // Lives for the whole process, sleeping in the kernel until something changes.
    std::thread([change, onChange = std::move(onChange)] {
        while (WaitForSingleObject(change, INFINITE) == WAIT_OBJECT_0) {
            onChange();
            if (!FindNextChangeNotification(change)) break;
        }
    }).detach();
}

} // namespace cup::platform
