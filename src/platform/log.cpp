#include "platform/log.h"

#include "platform/win.h"

#include <shlobj.h>
#include <oleidl.h> // DROPEFFECT_COPY

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

std::string narrow(const std::wstring& wide)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), n, nullptr, nullptr);
    return utf8;
}

// The last `bytes` of a file: whole lines only. "" if it can't be read.
std::string readTail(const std::filesystem::path& path, long bytes)
{
    FILE* in = _wfsopen(path.c_str(), L"rb", _SH_DENYNO);
    if (!in) return {};
    std::fseek(in, 0, SEEK_END);
    long size = std::ftell(in);
    long start = size > bytes ? size - bytes : 0;
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

// What Copy logs takes. The log is renamed to cup-island.old.log at a start
// once it's big, and that start is often the one after a freeze: then the
// freeze is all in the old file. So a short log comes with the old one's end
// (decision 144).
std::string readTail()
{
    std::lock_guard lock(g_mutex);
    std::string text = readTail(g_path, kCopyBytes);
    if (static_cast<long>(text.size()) < kCopyBytes / 2) {
        std::string old = readTail(g_path.parent_path() / L"cup-island.old.log", kCopyBytes - static_cast<long>(text.size()));
        if (!old.empty()) text = old + "--- cup-island.old.log ends; cup-island.log begins ---\n" + text;
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

// Copy logs puts a file on the clipboard, not text: pasted into a chat or a
// GitHub issue it arrives as an attachment, and in Explorer as the file
// itself, instead of 64 KB of lines in the message box (decision 159).
bool copyLogToClipboard(void* ownerHwnd)
{
    if (g_path.empty()) return false;
    std::filesystem::path file = g_path.parent_path() / L"cup-island-log.txt";
    std::string header = "Cup Island log: " + narrow(g_path.wstring()) + "\r\n\r\n";
    std::string tail = readTail();
    bool written = false;
    if (FILE* out = _wfopen(file.c_str(), L"wb")) {
        // A BOM, so Notepad and the browsers' previews read it as UTF-8.
        written = std::fputs("\xEF\xBB\xBF", out) >= 0 && std::fputs(header.c_str(), out) >= 0 &&
                  std::fwrite(tail.data(), 1, tail.size(), out) == tail.size();
        written = std::fclose(out) == 0 && written;
    }
    if (!written) {
        logLine("logs: could not write %ls", file.c_str());
        return false;
    }

    // CF_HDROP: a DROPFILES header, then the path, then an empty one ending the list.
    std::wstring path = file.wstring();
    size_t bytes = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);
    HGLOBAL drop = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!drop) return false;
    if (auto* files = static_cast<DROPFILES*>(GlobalLock(drop))) {
        files->pFiles = sizeof(DROPFILES);
        files->fWide = TRUE;
        std::memcpy(reinterpret_cast<char*>(files) + sizeof(DROPFILES), path.c_str(), path.size() * sizeof(wchar_t));
        GlobalUnlock(drop);
    }
    // Explorer pastes a copy, not a move, of a file that stays ours.
    HGLOBAL effect = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
    if (effect) {
        if (auto* value = static_cast<DWORD*>(GlobalLock(effect))) {
            *value = DROPEFFECT_COPY;
            GlobalUnlock(effect);
        }
    }

    // OpenClipboard needs a window: with none, SetClipboardData fails.
    bool copied = false;
    if (OpenClipboard(static_cast<HWND>(ownerHwnd))) {
        EmptyClipboard();
        copied = SetClipboardData(CF_HDROP, drop) != nullptr;
        if (copied) drop = nullptr; // the clipboard owns it now
        UINT effectFormat = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
        if (copied && effect && effectFormat && SetClipboardData(effectFormat, effect)) effect = nullptr;
        CloseClipboard();
    }
    if (drop) GlobalFree(drop);
    if (effect) GlobalFree(effect);
    logLine("logs %s to the clipboard as %ls", copied ? "copied" : "could not be copied", file.c_str());
    return copied;
}

} // namespace cup::platform
