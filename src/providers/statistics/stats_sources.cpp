// The plain looks: cores, the CPU's times, memory, the system drive
// (decision 173). Every API here is Windows 7 or older, so none needs a
// runtime lookup (decision 015).
#include "providers/statistics/stats_sources.h"

#include "platform/win.h"

#include <algorithm>

namespace cup::stats {
namespace {

std::uint64_t ticks(const FILETIME& time) // 100 ns
{
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
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

std::uint32_t coreCount()
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

std::optional<Space> memory()
{
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status) || status.ullTotalPhys == 0) return std::nullopt;
    // In use is what isn't available (standby and free), as Task Manager counts it.
    return Space{ .used = status.ullTotalPhys - std::min(status.ullAvailPhys, status.ullTotalPhys), .total = status.ullTotalPhys };
}

std::optional<Space> systemDrive()
{
    wchar_t windows[MAX_PATH] = {};
    wchar_t letter = GetWindowsDirectoryW(windows, MAX_PATH) > 0 ? windows[0] : L'C';
    const wchar_t root[] = { letter, L':', L'\\', 0 };
    ULARGE_INTEGER freeForUs{}, total{}, free{};
    if (!GetDiskFreeSpaceExW(root, &freeForUs, &total, &free) || total.QuadPart == 0 || total.QuadPart < free.QuadPart) return std::nullopt;
    return Space{ .used = total.QuadPart - free.QuadPart, .total = total.QuadPart };
}

} // namespace cup::stats
