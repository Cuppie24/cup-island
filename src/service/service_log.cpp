#include "service/service_log.h"

#include "service/service_win.h"

#include <shlobj.h>

#include <share.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <mutex>

namespace cup::service {
namespace {

std::mutex g_lock;
std::FILE* g_file = nullptr;
bool g_echo = false;
constexpr std::uintmax_t kMaxLogBytes = 1024 * 1024;

} // namespace

std::filesystem::path dataFolder()
{
    PWSTR base = nullptr;
    std::filesystem::path folder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base)))
        folder = std::filesystem::path(base) / L"CupIsland" / L"index-service";
    CoTaskMemFree(base);
    return folder;
}

void startServiceLog(const std::filesystem::path& folder, bool echo)
{
    std::lock_guard guard(g_lock);
    g_echo = echo;
    if (folder.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    std::filesystem::path log = folder / L"index.log";
    if (std::filesystem::file_size(log, ec) > kMaxLogBytes && !ec)
        std::filesystem::rename(log, folder / L"index.old.log", ec);
    g_file = _wfsopen(log.c_str(), L"ab", _SH_DENYWR);
}

void serviceLog(const char* format, ...)
{
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);

    SYSTEMTIME t;
    GetLocalTime(&t);
    char line[1100];
    std::snprintf(line, sizeof line, "%04u-%02u-%02u %02u:%02u:%02u.%03u %s\n", t.wYear, t.wMonth, t.wDay, t.wHour,
                  t.wMinute, t.wSecond, t.wMilliseconds, text);
    std::lock_guard guard(g_lock);
    if (g_file) {
        std::fputs(line, g_file);
        std::fflush(g_file);
    }
    if (g_echo) std::fputs(line, stdout);
}

} // namespace cup::service
