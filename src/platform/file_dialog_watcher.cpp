#include "platform/file_dialog_watcher.h"

#include "platform/log.h"
#include "platform/text_format.h"
#include "platform/window_name.h"

#include "core/file_dialog.h"
#include "core/tokens.h"

#include <dwmapi.h>

#include <string>
#include <utility>
#include <vector>

namespace cup::platform {
namespace {

constexpr std::size_t kMaxInside = 512; // a dialog's controls: far fewer; a stop for a window gone wrong

std::function<void(std::optional<FileDialogSeen>)> g_onChange;
HWND g_dialog{};                         // the one in front, or behind the island's own card
std::optional<FileDialogSeen> g_told;    // what onChange last heard
HWINEVENTHOOK g_lifeHook{}, g_moveHook{}; // on the dialog's thread only: other apps never wake us
UINT_PTR g_recheck = 0;

std::string classOf(HWND hwnd)
{
    char name[128]{};
    GetClassNameA(hwnd, name, 128); // class names are ASCII; reads no other process's memory
    return name;
}

BOOL CALLBACK collect(HWND child, LPARAM out)
{
    auto& windows = *reinterpret_cast<std::vector<HWND>*>(out);
    windows.push_back(child);
    return windows.size() < kMaxInside;
}

std::vector<HWND> inside(HWND hwnd)
{
    std::vector<HWND> windows;
    EnumChildWindows(hwnd, collect, reinterpret_cast<LPARAM>(&windows)); // every descendant, not just children
    return windows;
}

std::vector<std::string> classesInside(HWND hwnd)
{
    std::vector<std::string> classes;
    for (HWND child : inside(hwnd)) classes.push_back(classOf(child));
    return classes;
}

// The frame as it looks: GetWindowRect includes Windows 10's invisible
// resize border, and the island would hang a few pixels under the dialog.
Rect frameOf(HWND hwnd)
{
    RECT r{};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof r))) GetWindowRect(hwnd, &r);
    // Less the one-pixel border Windows draws just inside that frame: what
    // hangs from the dialog meets its body, without a line of border between.
    constexpr LONG kBorder = 1;
    return { static_cast<float>(r.left), static_cast<float>(r.top + kBorder), static_cast<float>(r.right - r.left),
             static_cast<float>(r.bottom - r.top - 2 * kBorder) };
}

void tell()
{
    std::optional<FileDialogSeen> now;
    if (g_dialog) now = FileDialogSeen{ g_dialog, frameOf(g_dialog) };
    if (now == g_told) return;
    g_told = now;
    g_onChange(now);
}

void unhook()
{
    if (g_lifeHook) UnhookWinEvent(std::exchange(g_lifeHook, nullptr));
    if (g_moveHook) UnhookWinEvent(std::exchange(g_moveHook, nullptr));
}

void CALLBACK onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object, LONG child, DWORD, DWORD);

void attach(HWND hwnd)
{
    if (hwnd != g_dialog) {
        unhook();
        g_dialog = hwnd;
        DWORD process = 0, thread = GetWindowThreadProcessId(hwnd, &process);
        // Closing (destroyed or hidden) and moving, of that thread's windows only.
        g_lifeHook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE, nullptr, onWinEvent, process, thread, WINEVENT_OUTOFCONTEXT);
        g_moveHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, onWinEvent, process, thread, WINEVENT_OUTOFCONTEXT);
        logLine("file dialog in front: %ls", describeWindow(hwnd).c_str());
    }
    tell();
}

void detach()
{
    if (!g_dialog) return;
    unhook();
    g_dialog = nullptr;
    logLine("file dialog gone");
    tell();
}

// A window the dialog opened: its own question ("Replace the file?"), a drop-down.
bool ownedByDialog(HWND hwnd)
{
    for (int depth = 0; hwnd && depth < 4; ++depth) {
        hwnd = GetWindow(hwnd, GW_OWNER);
        if (hwnd == g_dialog) return true;
    }
    return false;
}

void CALLBACK onRecheck(HWND, UINT, UINT_PTR, DWORD);

void evaluate(bool mayRecheck)
{
    if (g_dialog && (!IsWindow(g_dialog) || !IsWindowVisible(g_dialog))) detach();
    HWND front = GetForegroundWindow();
    if (!front) return; // between two windows: the next event says
    DWORD process = 0;
    GetWindowThreadProcessId(front, &process);
    // The island's own card took the keys: the dialog behind it is still the one.
    if (process == GetCurrentProcessId() || front == g_dialog || (g_dialog && ownedByDialog(front))) {
        tell();
        return;
    }
    std::string cls = classOf(front);
    std::vector<std::string> classes = classesInside(front);
    if (isFileDialog(cls, classes)) {
        attach(front);
        return;
    }
    detach();
    // An old-style dialog builds its file view after it's shown: once more, shortly.
    if (mayRecheck && mayBecomeFileDialog(cls, classes) && !g_recheck)
        g_recheck = SetTimer(nullptr, 0, static_cast<UINT>(tokens().pickerRecheck.count()), onRecheck);
}

void CALLBACK onRecheck(HWND, UINT, UINT_PTR id, DWORD)
{
    KillTimer(nullptr, id);
    g_recheck = 0;
    evaluate(false);
}

void CALLBACK onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object, LONG child, DWORD, DWORD)
{
    if (event == EVENT_SYSTEM_FOREGROUND) {
        evaluate(true);
        return;
    }
    // The dialog itself, not its controls, carets or the cursor.
    if (hwnd != g_dialog || object != OBJID_WINDOW || child != CHILDID_SELF) return;
    if (event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_HIDE) detach();
    else if (event == EVENT_OBJECT_LOCATIONCHANGE) tell();
}

// Its "File name" field: the first visible Edit that core says is one.
HWND fileNameField(HWND dialog)
{
    for (HWND control : inside(dialog)) {
        if (!IsWindowVisible(control)) continue;
        std::vector<std::string> chain; // from the dialog's child down to the control
        for (HWND at = control; at && at != dialog; at = GetAncestor(at, GA_PARENT)) chain.insert(chain.begin(), classOf(at));
        if (isFileNameField(chain)) return control;
    }
    return nullptr;
}

} // namespace

void watchFileDialogs(std::function<void(std::optional<FileDialogSeen>)> onChange)
{
    g_onChange = std::move(onChange);
    // Out of context: delivered through this (UI) thread's message loop.
    SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, onWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    evaluate(true); // started with one already open
}

void focusFileDialog()
{
    if (g_dialog && IsWindow(g_dialog)) SetForegroundWindow(g_dialog);
}

bool giveToFileDialog(const std::string& path)
{
    if (!g_dialog || !IsWindow(g_dialog)) return false;
    HWND field = fileNameField(g_dialog);
    if (!field) {
        logLine("file dialog: no File name field found in %ls", describeWindow(g_dialog).c_str());
        return false;
    }
    // Into another process, which may hang (decision 144): it gets
    // pickerSendTimeout, and a hung one is given up on at once.
    std::wstring text = widen(path);
    DWORD_PTR taken = 0;
    UINT wait = static_cast<UINT>(tokens().pickerSendTimeout.count());
    if (!SendMessageTimeoutW(field, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()), SMTO_ABORTIFHUNG, wait, &taken) || !taken) {
        logLine("file dialog: didn't take the path (error %lu); an elevated app's doesn't", GetLastError());
        SetForegroundWindow(g_dialog); // in front all the same: the path is pasted there by hand
        return false;
    }
    SetForegroundWindow(g_dialog); // the island's card had the keys: back to the dialog, which closes or shows the folder
    // Its Open / Save button, posted: the dialog may ask its own question
    // next ("Replace the file?"), and a sent message would wait for the answer.
    return PostMessageW(g_dialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(g_dialog, IDOK))) != 0;
}

} // namespace cup::platform
