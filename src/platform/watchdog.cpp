#include "platform/watchdog.h"

#include "platform/log.h"

#include <dbghelp.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <string>
#include <thread>

namespace cup::platform {
namespace {

// The check runs this often, and the UI thread gets this long to answer.
// A few seconds keeps the watchdog nearly free while idle (TR §8).
constexpr DWORD kCheckEveryMs = 5000;
constexpr UINT kAnswerWithinMs = 5000;

std::filesystem::path dumpPath(const char* kind)
{
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t name[64];
    swprintf_s(name, L"cup-island-%hs-%04u%02u%02u-%02u%02u%02u.dmp", kind, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return logFolder() / name;
}

// Every thread's stack and the loaded modules: enough to see where each thread
// is, small enough to send (well under a few MB).
void writeDump(const char* kind, EXCEPTION_POINTERS* exception)
{
    std::filesystem::path path = dumpPath(kind);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        logLine("can't create %ls (0x%08lx)", path.c_str(), GetLastError());
        return;
    }
    MINIDUMP_EXCEPTION_INFORMATION info{ GetCurrentThreadId(), exception, FALSE };
    auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules | MiniDumpWithIndirectlyReferencedMemory);
    BOOL written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, exception ? &info : nullptr, nullptr, nullptr);
    CloseHandle(file);
    if (written) logLine("dump written: %ls", path.c_str());
    else logLine("dump failed (0x%08lx)", GetLastError());
}

// The first handler to see a crash reports it; others it trips on the way
// down (terminate -> abort -> SIGABRT) stay quiet.
std::atomic<bool> g_reported{ false };
bool firstReport() { return !g_reported.exchange(true); }

LONG WINAPI onUnhandledException(EXCEPTION_POINTERS* exception)
{
    if (!firstReport()) return EXCEPTION_CONTINUE_SEARCH;
    logLine("CRASH: exception 0x%08lx at %p", exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    writeDump("crash", exception);
    return EXCEPTION_CONTINUE_SEARCH; // then Windows' own crash handling
}

// An exception escaping a noexcept function or a fire_and_forget coroutine ends up here.
void onTerminate()
{
    if (firstReport()) {
        logLine("CRASH: std::terminate (an uncaught C++ exception)");
        writeDump("terminate", nullptr);
    }
    std::abort();
}

// These three end the process without passing the unhandled-exception
// filter: before, they left no trace at all.
void onAbort(int)
{
    if (!firstReport()) return;
    logLine("CRASH: abort()");
    writeDump("abort", nullptr);
} // returning lets abort() end the process

void onPureCall()
{
    if (firstReport()) {
        logLine("CRASH: pure virtual function call");
        writeDump("purecall", nullptr);
    }
    std::abort();
}

void onInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t)
{
    if (firstReport()) {
        logLine("CRASH: invalid parameter passed to a C runtime function");
        writeDump("invalid-parameter", nullptr);
    }
    std::abort();
}

} // namespace

void watchForCrashes()
{
    SetUnhandledExceptionFilter(onUnhandledException);
    std::set_terminate(onTerminate);
    std::signal(SIGABRT, onAbort);
    _set_purecall_handler(onPureCall);
    _set_invalid_parameter_handler(onInvalidParameter);
    // Keeps room on this (the UI) thread's stack after an overflow, so the
    // filter above can still log and dump instead of dying silently.
    ULONG reserve = 64 * 1024;
    SetThreadStackGuarantee(&reserve);
}

void watchForHangs(HWND uiWindow)
{
    std::thread([uiWindow] {
        bool hung = false;
        auto hungSince = std::chrono::steady_clock::now();
        for (;;) {
            Sleep(kCheckEveryMs);
            if (!IsWindow(uiWindow)) return; // shutting down
            DWORD_PTR result = 0;
            bool answered = SendMessageTimeoutW(uiWindow, WM_NULL, 0, 0, SMTO_NORMAL, kAnswerWithinMs, &result) != 0;
            if (!answered && !IsWindow(uiWindow)) return;
            if (!answered && !hung) {
                hung = true;
                hungSince = std::chrono::steady_clock::now();
                logLine("HANG: the UI thread hasn't answered for %u ms; dumping every thread's stack", kAnswerWithinMs);
                writeDump("hang", nullptr); // once per hang
            } else if (answered && hung) {
                hung = false;
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - hungSince).count();
                logLine("the UI thread is answering again (stuck for about %lld s)", static_cast<long long>(seconds) + kAnswerWithinMs / 1000);
            }
        }
    }).detach();
}

} // namespace cup::platform
