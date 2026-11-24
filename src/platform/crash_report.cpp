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
    return std::wstring(text.data());
}

// The record's own fields (the program, the module and code it failed in,
// the offset), as XML. EvtFormatMessage is documented to work without the
// provider's metadata only for events forwarded from another PC, and no log
// has ever shown a record it read: three silent crashes in a row came out
// as "no crash record" (decision 178). This needs no metadata.
std::wstring fieldsOf(EVT_HANDLE event)
{
    DWORD bytes = 0, properties = 0;
    EvtRender(nullptr, event, EvtRenderEventXml, 0, nullptr, &bytes, &properties);
    if (bytes == 0) return {};
    std::vector<wchar_t> text(bytes / sizeof(wchar_t) + 1);
    if (!EvtRender(nullptr, event, EvtRenderEventXml, static_cast<DWORD>(text.size() * sizeof(wchar_t)), text.data(), &bytes, &properties)) return {};
    std::wstring xml(text.data());
    // After <System> (the provider, the time): what a log line has room for.
    auto data = xml.find(L"<EventData");
    return data == std::wstring::npos ? xml : xml.substr(data);
}

std::wstring recordOf(EVT_HANDLE event)
{
    std::wstring record = messageOf(event);
    if (record.empty()) record = fieldsOf(event);
    for (wchar_t& ch : record)
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' '; // one log line
    return record;
}

// Windows writes its record of a fail-fast a few seconds after the process
// ends (WerFault collects it first), and a quick restart looked before that:
// "no crash record" for what was a crash (decision 158). So it looks again,
// for a while. Fixed, not tokens: this runs before tokens.json is read.
constexpr DWORD kRecordRetryMs = 2000;
constexpr int kRecordTries = 15; // about 30 s

// Windows' "Application Error" (a crash, fail-fasts included) and
// "Application Hang" (closed while not responding) records for our exe
// since the previous run started. Returns how many it logged; -1 if the log can't be read.
int logWindowsRecordsOnce(unsigned long long since)
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
        return -1;
    }
    int found = 0;
    EVT_HANDLE events[16];
    DWORD returned = 0;
    while (EvtNext(results, 16, events, 2000, 0, &returned)) {
        for (DWORD i = 0; i < returned; ++i) {
            std::wstring message = recordOf(events[i]);
            EvtClose(events[i]);
            if (lowered(message).find(L"cupisland.exe") == std::wstring::npos) continue;
            logLine("Windows recorded: %ls", message.c_str());
            ++found;
        }
    }
    EvtClose(results);
    return found;
}

void logWindowsRecords(unsigned long long since)
{
    for (int attempt = 0; attempt < kRecordTries; ++attempt) {
        if (attempt > 0) Sleep(kRecordRetryMs);
        int found = logWindowsRecordsOnce(since);
        if (found != 0) return; // logged, or the event log can't be read at all
    }
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
