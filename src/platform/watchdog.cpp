#include "platform/watchdog.h"

#include "platform/log.h"

#include <dbghelp.h>
#include <winrt/base.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <string>
#include <thread>

namespace cup::platform {
namespace {

// The UI thread gets this long to answer a ping; the next goes out once it
// has. A few seconds keeps the watchdog nearly free while idle (TR §8).
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

std::atomic<unsigned> g_pingsAnswered{ 0 }; // the UI thread counts the pings it handled

LONG WINAPI onUnhandledException(EXCEPTION_POINTERS* exception)
{
    if (!firstReport()) return EXCEPTION_CONTINUE_SEARCH;
    logLine("CRASH: exception 0x%08lx at %p", exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    writeDump("crash", exception);
    return EXCEPTION_CONTINUE_SEARCH; // then Windows' own crash handling
}

// Windows ends the process itself when it finds the heap corrupted, past the
// filter above: the island vanished with nothing in the log (decision 178).
// A vectored handler is called before that. Logged only: a dump needs the
// heap, and where it's found isn't where it was done (some thread wrote
// past its memory earlier).
constexpr DWORD kHeapCorruption = 0xC0000374; // STATUS_HEAP_CORRUPTION

LONG WINAPI onFirstChance(EXCEPTION_POINTERS* exception)
{
    if (exception->ExceptionRecord->ExceptionCode == kHeapCorruption && firstReport())
        logLine("CRASH: Windows found the heap corrupted (0x%08lx) at %p: memory was overwritten earlier, by any thread",
                exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_CONTINUE_SEARCH;
}

// An exception escaping a noexcept function ends up here. Not one leaving a
// winrt::fire_and_forget: C++/WinRT fail-fasts on that, past this (decision 158).
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
    AddVectoredExceptionHandler(/* first */ 1, onFirstChance);
    std::set_terminate(onTerminate);
    std::signal(SIGABRT, onAbort);
    _set_purecall_handler(onPureCall);
    _set_invalid_parameter_handler(onInvalidParameter);
    // Keeps room on this (the UI) thread's stack after an overflow, so the
    // filter above can still log and dump instead of dying silently.
    ULONG reserve = 64 * 1024;
    SetThreadStackGuarantee(&reserve);
}

void guardedTask(const char* what, const std::function<void()>& work)
{
    try {
        work();
    } catch (winrt::hresult_error const& e) {
        logLine("WinRT error %s: 0x%08x %ls", what, static_cast<unsigned int>(e.code()), e.message().c_str());
    } catch (std::exception const& e) {
        logLine("Exception %s: %s", what, e.what());
    } catch (...) {
        logLine("Unknown exception %s", what);
    }
}

UINT watchdogPing()
{
    static const UINT message = RegisterWindowMessageW(L"CupIsland.WatchdogPing");
    return message;
}

void answerWatchdog()
{
    ++g_pingsAnswered;
}

void watchForHangs(HWND uiWindow)
{
    std::thread([uiWindow] {
        bool hung = false;
        bool pinged = false; // one ping at a time: a hung thread isn't sent a pile of them
        unsigned answeredBefore = 0;
        auto hungSince = std::chrono::steady_clock::now();
        for (;;) {
            if (!pinged) {
                answeredBefore = g_pingsAnswered;
                pinged = PostMessageW(uiWindow, watchdogPing(), 0, 0) != FALSE;
            }
            Sleep(kAnswerWithinMs);
            if (!IsWindow(uiWindow)) return; // shutting down
            bool answered = pinged && g_pingsAnswered != answeredBefore;
            if (answered) pinged = false;
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
