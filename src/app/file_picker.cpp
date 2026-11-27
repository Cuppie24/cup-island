#include "app/file_picker.h"

#include "core/file_dialog.h"
#include "core/lift.h"
#include "core/tokens.h"
#include "platform/file_dialog_watcher.h"
#include "platform/watchdog.h"

#include <optional>
#include <utility>

namespace cup::app {
namespace {

FilePickerHooks g_hooks;
std::optional<platform::FileDialogSeen> g_dialog; // the one in front
std::optional<DialogPlace> g_place;               // where the island's layers are
bool g_cardOpen = false;                          // as of the last settle
UINT_PTR g_hideTimer = 0;                         // faded out, its windows hide when this fires
UINT_PTR g_movedTimer = 0;                        // moved with the dialog: layersMoved when it has come to rest

void CALLBACK onMovedTimer(HWND, UINT, UINT_PTR id, DWORD)
{
    KillTimer(nullptr, id);
    g_movedTimer = 0;
    platform::guardedTask("after the file dialog moved", [] { g_hooks.onIsland([] { g_hooks.layersMoved(); }); });
}

// By the dialog, as core places it. On the dialog's island.
void place()
{
    if (!g_dialog) return;
    DialogPlace want = placeByDialog(g_dialog->bounds, g_hooks.workAreaOf(g_dialog->bounds), tokens().canvas, modeSize(Mode::Compact).y,
                                     isExpanded(g_hooks.state()));
    if (g_place == want) return; // where it is: a move would start liquid glass over
    g_place = want;
    g_hooks.moveTo(want.origin);
    // A dragged dialog moves many times a second: liquid glass, which starts
    // a new capture of the screen each time, waits until it rests. An open
    // card (it only moves as it opens or closes) has it at once.
    if (g_movedTimer) KillTimer(nullptr, std::exchange(g_movedTimer, 0));
    if (isExpanded(g_hooks.state())) g_hooks.layersMoved();
    else g_movedTimer = SetTimer(nullptr, 0, static_cast<UINT>(tokens().pickerRecheck.count()), onMovedTimer);
}

void CALLBACK onHideTimer(HWND, UINT, UINT_PTR id, DWORD)
{
    KillTimer(nullptr, id);
    g_hideTimer = 0;
    if (g_dialog) return; // one came back meanwhile
    platform::guardedTask("hiding the file dialog's island", [] { g_hooks.onIsland([] { g_hooks.setVisible(false); }); });
}

void show()
{
    if (g_hideTimer) KillTimer(nullptr, std::exchange(g_hideTimer, 0));
    g_hooks.setVisible(true);
    g_hooks.fade(true);
}

void hide()
{
    g_place.reset();
    g_hooks.fade(false);
    if (g_hideTimer) KillTimer(nullptr, g_hideTimer);
    g_hideTimer = SetTimer(nullptr, 0, static_cast<UINT>(tokens().pickerFade.count()), onHideTimer);
}

void onDialog(std::optional<platform::FileDialogSeen> seen)
{
    bool came = seen && !g_dialog, went = !seen && g_dialog;
    g_dialog = seen;
    g_hooks.onIsland([&] {
        if (went) {
            if (isExpanded(g_hooks.state())) g_hooks.dispatch(EscapePressed{}, "file dialog gone"); // nothing left to give a file to
            hide();
            return;
        }
        place(); // it came, or moved: the island moves with it, as one piece
        if (came) show();
    });
}

} // namespace

void watchFilePicker(FilePickerHooks hooks)
{
    g_hooks = std::move(hooks);
    platform::watchFileDialogs(onDialog);
}

void settleFilePicker()
{
    if (!g_hooks.state) return;
    bool open = isExpanded(g_hooks.state());
    bool closed = std::exchange(g_cardOpen, open) && !open;
    place();
    // Closed with the keys still here (Esc, a pick the dialog refused): the
    // user goes on in the dialog. Closed by a click elsewhere, that has them.
    if (closed && g_dialog && g_hooks.hasKeys()) platform::focusFileDialog();
}

void refitFilePicker()
{
    g_place.reset();
    place();
}

bool filePickerShortcut()
{
    if (!g_dialog || !g_hooks.state) return false;
    g_hooks.onIsland([] {
        if (isExpanded(g_hooks.state())) {
            g_hooks.dispatch(EscapePressed{}, "shortcut"); // again: it closes, as Search's does
            return;
        }
        g_hooks.dispatch(PillClicked{}, "shortcut"); // as a click on its pill
        if (isExpanded(g_hooks.state())) g_hooks.focus();
    });
    return true;
}

} // namespace cup::app
