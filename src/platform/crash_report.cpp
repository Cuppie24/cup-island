#include "platform/crash_report.h"

#include "platform/log.h"
#include "platform/win.h"

#include <winevt.h>

#include <cwctype>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace cup::platform {
namespace {

// Holds "pid start-time" while the app runs; a clean exit deletes it.
std::filesystem::path markerPath() { return logFolder() / L"running.txt"; }

struct PreviousRun {
    unsigned long pid = 0;
    unsigned long long started = 0; // FILETIME, UTC
};

std::wstring lowered(std::wstring text)
{
    for (wchar_t& ch : text) ch = static_cast<wchar_t>(std::towlower(ch));
    return text;
}

std::wstring messageOf(EVT_HANDLE event)
{
    DWORD needed = 0;
    EvtFormatMessage(nullptr, event, 0, 0, nullptr, EvtFormatMessageEvent, 0, nullptr, &needed);
    if (needed == 0) return {};
    std::vector<wchar_t> text(needed);
    if (!EvtFormatMessage(nullptr, event, 0, 0, nullptr, EvtFormatMessageEvent, needed, text.data(), &needed)) return {};
    std::wstring message(text.data());
    for (wchar_t& ch : message)
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' '; // one log line
    return message;
}

// Windows' "Application Error" (a crash, fail-fasts included) and
// "Application Hang" (closed while not responding) records for our exe
// since the previous run started.
void logWindowsRecords(unsigned long long since)
{
    FILETIME fileTime{ static_cast<DWORD>(since), static_cast<DWORD>(since >> 32) };
    SYSTEMTIME t{};
    FileTimeToSystemTime(&fileTime, &t);
    wchar_t query[400];
    swprintf_s(query,
               L"*[System[Provider[@Name='Application Error' or @Name='Application Hang'] and (EventID=1000 or EventID=1002)"
               L" and TimeCreated[@SystemTime>='%04u-%02u-%02uT%02u:%02u:%02u.000Z']]]",
               t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    EVT_HANDLE results = EvtQuery(nullptr, L"Application", query, EvtQueryChannelPath | EvtQueryForwardDirection);
    if (!results) {
        logLine("can't read Windows' Application event log (0x%08lx)", GetLastError());
        return;
    }
    int found = 0;
    EVT_HANDLE events[16];
    DWORD returned = 0;
    while (EvtNext(results, 16, events, 2000, 0, &returned)) {
        for (DWORD i = 0; i < returned; ++i) {
            std::wstring message = messageOf(events[i]);
            EvtClose(events[i]);
            if (lowered(message).find(L"cupisland.exe") == std::wstring::npos) continue;
            logLine("Windows recorded: %ls", message.c_str());
            ++found;
        }
    }
    EvtClose(results);
    if (found == 0)
        logLine("Windows has no crash record for it: it was ended from outside (Task Manager, sign-out, power loss) "
                "or crashed without a report");
}

} // namespace

void reportPreviousCrash()
{
    std::filesystem::path marker = markerPath();
    if (marker.empty()) return;
    PreviousRun previous;
    bool unclean = false;
    if (std::ifstream in{ marker }; in) unclean = static_cast<bool>(in >> previous.pid >> previous.started);
    if (unclean) {
        logLine("PREVIOUS RUN ENDED UNEXPECTEDLY (pid %lu): checking Windows' crash records", previous.pid);
        std::thread([since = previous.started] { logWindowsRecords(since); }).detach();
    }

    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    unsigned long long started = (static_cast<unsigned long long>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
    std::ofstream{ marker } << GetCurrentProcessId() << ' ' << started << '\n';
}

void markCleanExit()
{
    std::error_code error;
    std::filesystem::remove(markerPath(), error);
}

} // namespace cup::platform
