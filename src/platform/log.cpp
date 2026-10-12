#include "platform/log.h"

#include "platform/win.h"

#include <shlobj.h>

#include <clocale>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <share.h>
#include <string>

namespace cup::platform {
namespace {

constexpr std::uintmax_t kMaxSize = 2 * 1024 * 1024;
constexpr long kCopyBytes = 64 * 1024;

std::mutex g_mutex; // one line at a time, from any thread
FILE* g_file = nullptr;
std::filesystem::path g_path;

std::wstring widen(const std::string& utf8)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), n);
    return wide;
}

// The tail of the log file: whole lines only.
std::string readTail()
{
    std::lock_guard lock(g_mutex);
    FILE* in = _wfsopen(g_path.c_str(), L"rb", _SH_DENYNO);
    if (!in) return {};
    std::fseek(in, 0, SEEK_END);
    long size = std::ftell(in);
    long start = size > kCopyBytes ? size - kCopyBytes : 0;
    std::fseek(in, start, SEEK_SET);
    std::string text(static_cast<size_t>(size - start), '\0');
    text.resize(std::fread(text.data(), 1, text.size(), in));
    std::fclose(in);
    if (start > 0) {
        auto firstLineEnd = text.find('\n');
        text.erase(0, firstLineEnd == std::string::npos ? 0 : firstLineEnd + 1);
    }
    return text;
}

} // namespace

std::filesystem::path logFolder()
{
    PWSTR base = nullptr;
    std::filesystem::path folder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base))) folder = std::filesystem::path(base) / L"CupIsland" / L"logs";
    CoTaskMemFree(base);
    return folder;
}

void startLog()
{
    // %ls (Windows' messages, window titles, paths) is converted with the C
    // locale's code page, and the default "C" one can't encode Cyrillic, CJK,
    // emoji: vsnprintf fails and the whole line came out "(unprintable)",
    // error code included. UTF-8 for text only (LC_CTYPE): numbers keep their
    // "." whatever the user's region says.
    std::setlocale(LC_CTYPE, ".UTF-8");
    SetConsoleOutputCP(CP_UTF8); // the Debug console shows the same text; no console: does nothing
    std::filesystem::path folder = logFolder();
    if (folder.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    g_path = folder / L"cup-island.log";
    auto size = std::filesystem::file_size(g_path, error);
    if (!error && size > kMaxSize) std::filesystem::rename(g_path, folder / L"cup-island.old.log", error);
    // Shared, so the file can be opened in an editor while the app runs.
    g_file = _wfsopen(g_path.c_str(), L"a", _SH_DENYNO);
}

void logLine(const char* format, ...)
{
    char message[1024];
    va_list args;
    va_start(args, format);
    int written = std::vsnprintf(message, sizeof message, format, args);
    va_end(args);
    if (written < 0) std::snprintf(message, sizeof message, "(unprintable) %s", format); // e.g. a %ls that won't convert

    SYSTEMTIME t{};
    GetLocalTime(&t);
    char line[1200];
    std::snprintf(line, sizeof line, "%04u-%02u-%02u %02u:%02u:%02u.%03u [%5lu] %s\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                  t.wSecond, t.wMilliseconds, GetCurrentThreadId(), message);

    std::lock_guard lock(g_mutex);
    std::fputs(line, stdout); // the Debug console; goes nowhere in Release
    std::fflush(stdout);
    if (g_file) {
        std::fputs(line, g_file);
        std::fflush(g_file);
    }
}

bool copyLogToClipboard(void* ownerHwnd)
{
    if (g_path.empty()) return false;
    std::wstring text = L"Cup Island log: " + g_path.wstring() + L"\r\n\r\n" + widen(readTail());

    // OpenClipboard needs a window: with none, SetClipboardData fails.
    if (!OpenClipboard(static_cast<HWND>(ownerHwnd))) return false;
    bool copied = false;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* target = GlobalLock(memory)) {
            std::memcpy(target, text.c_str(), bytes);
            GlobalUnlock(memory);
            copied = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
        }
        if (!copied) GlobalFree(memory); // owned by the clipboard only on success
    }
    CloseClipboard();
    logLine("logs %s to the clipboard", copied ? "copied" : "could not be copied");
    return copied;
}

} // namespace cup::platform
