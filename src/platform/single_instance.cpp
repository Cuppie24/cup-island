#include "platform/single_instance.h"

#include "platform/interaction_layer.h"

#include <cstdio>
#include <string>

namespace cup::platform {
namespace {

// The installer's AppId: the app's permanent identity. "Local\" = per sign-in
// session, so with fast user switching each user gets their own island.
constexpr wchar_t kLockName[] = L"Local\\CupIsland-6CB3203D-53EC-4816-944E-3BC5A4DD3489";

} // namespace

UINT showYourselfMessage()
{
    static const UINT message = RegisterWindowMessageW(L"CupIsland.ShowYourself");
    return message;
}

bool becomeTheOnlyInstance()
{
    // Never closed: Windows releases it when this process ends, crashes
    // included, so a dead island can't block the next start.
    SetLastError(ERROR_SUCCESS);
    HANDLE lock = CreateMutexW(nullptr, FALSE, kLockName);
    if (!lock) return true; // can't tell (shouldn't happen): better two islands than none
    if (GetLastError() != ERROR_ALREADY_EXISTS) return true;

    std::wstring note = L"Cup Island is already running";
    if (HWND running = FindWindowW(kInteractionWindowClass, nullptr)) {
        DWORD pid = 0;
        GetWindowThreadProcessId(running, &pid);
        AllowSetForegroundWindow(pid); // so it may take focus when it opens Home
        PostMessageW(running, showYourselfMessage(), 0, 0);
        note += L" (pid " + std::to_wstring(pid) + L"); it was asked to show itself.";
    } else {
        note += L"; it's still starting up."; // its window doesn't exist yet
    }
    // Debug builds have a console; the debugger's Output window shows the rest.
    std::printf("%ls\n", note.c_str());
    OutputDebugStringW((note + L"\n").c_str());
    CloseHandle(lock);
    return false;
}

} // namespace cup::platform
