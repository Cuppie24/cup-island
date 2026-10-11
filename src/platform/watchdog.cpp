#include "platform/watchdog.h"

#include "platform/log.h"

#include <dbghelp.h>

#include <chrono>
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

LONG WINAPI onUnhandledException(EXCEPTION_POINTERS* exception)
{
    logLine("CRASH: exception 0x%08lx at %p", exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    writeDump("crash", exception);
    return EXCEPTION_CONTINUE_SEARCH; // then Windows' own crash handling
}

// An exception escaping a noexcept function or a fire_and_forget coroutine ends up here.
void onTerminate()
{
    logLine("CRASH: std::terminate (an uncaught C++ exception)");
    writeDump("terminate", nullptr);
    std::abort();
}

} // namespace

void watchForCrashes()
{
    SetUnhandledExceptionFilter(onUnhandledException);
    std::set_terminate(onTerminate);
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
