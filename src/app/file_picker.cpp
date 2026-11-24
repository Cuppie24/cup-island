#include "app/file_picker.h"

#include "core/file_dialog.h"
#include "core/lift.h"
#include "core/tokens.h"
#include "platform/file_dialog_watcher.h"
#include "platform/picker_pill.h"
#include "platform/watchdog.h"
#include "providers/search/search_provider.h"

#include <utility>

namespace cup::app {
namespace {

using Stage = Picker::Stage;

FilePickerHooks g_hooks;
std::optional<platform::FileDialogSeen> g_dialog; // the one in front
bool g_lent = false;      // the island's layers hang under it
bool g_seenThere = false; // ...and the island is seen there, in the pill's place: the pill isn't
bool g_settling = false;  // settle() dispatches, and every dispatch ends in it
// One wait at a time: for the island to be drawn under the dialog before it's
// shown opening, or for it to have closed there before it's taken home.
UINT_PTR g_timer = 0;
std::function<void()> g_after;

const std::optional<Picker>& picker() { return g_hooks.state().picker; } // core's word on the loan

void CALLBACK onTimer(HWND, UINT, UINT_PTR id, DWORD)
{
    KillTimer(nullptr, id);
    g_timer = 0;
    if (auto then = std::exchange(g_after, nullptr)) platform::guardedTask("in the file picker's timer", then);
}

void after(Ms wait, std::function<void()> then)
{
    if (g_timer) KillTimer(nullptr, g_timer);
    g_after = std::move(then);
    g_timer = SetTimer(nullptr, 0, static_cast<UINT>(wait.count()), onTimer);
}

void cancelWait()
{
    if (g_timer) KillTimer(nullptr, std::exchange(g_timer, 0));
    g_after = nullptr;
}

// The pill: by the dialog while one is in front and the island isn't seen in its place.
void placePill(bool atOnce = false)
{
    if (g_dialog && !g_seenThere) {
        PillPlace place = pillPlace(g_dialog->bounds, g_hooks.displayOf(g_dialog->bounds).workArea, modeSize(Mode::Idle));
        platform::showPickerPill(place.frame, place.above, atOnce);
    } else {
        platform::hidePickerPill(atOnce);
    }
}

// The layers under the dialog; where its middle is in them, for core.
float hangLayers()
{
    PickerDisplay display = g_hooks.displayOf(g_dialog->bounds);
    Hang hang = hangUnder(g_dialog->bounds, display.workArea, /* cardOpen */ true);
    g_lent = true;
    g_hooks.hang(HangPlace{ display.id, hang.top });
    return hang.centre - g_hooks.layerLeft();
}

// Home: the layers back at the top of the island's screen, the island fading in there.
// Unseen on the way: main's dispatch veils it whenever core's Picker comes or goes.
void sendHome()
{
    cancelWait();
    g_lent = g_seenThere = false;
    g_hooks.hang(std::nullopt);
    g_hooks.unveil(/* fade */ true);
}

// The island has closed back to a pill under the dialog: the dialog's own
// pill takes its place at once, and the island goes home.
void leave()
{
    if (!picker() || picker()->stage != Stage::Leaving) return;
    g_settling = true;
    g_hooks.veil();
    g_seenThere = false;
    placePill(/* atOnce */ true);
    g_hooks.dispatch(PickerLeft{}, "back from the file dialog");
    sendHome();
    g_settling = false;
    platform::focusFileDialog(); // the island had the keys for its card: the user goes on in the dialog
}

// Put under the dialog and drawn there, still unseen: it takes the pill's
// place at once and opens the picking card from it.
void open()
{
    if (!picker() || picker()->stage != Stage::Arriving) return;
    g_seenThere = true;
    g_hooks.unveil(/* fade */ false);
    placePill(/* atOnce */ true);
    g_hooks.dispatch(PickerOpened{}, "file dialog's pill");
    if (picker() && picker()->stage == Stage::Open) g_hooks.focus();
}

void settle()
{
    if (g_settling || !g_hooks.state) return;
    g_settling = true;
    if (g_lent && !picker()) sendHome(); // dropped outright (the dialog went before the card opened, the island hid)
    else if (g_lent && picker()->stage == Stage::Leaving && !g_timer)
        after(g_hooks.morphTime() + tokens().pickerSwap, leave); // once it has closed to a pill there
    placePill();
    g_settling = false;
}

// The pill was clicked: the island goes under the dialog unseen, as it rests; shown a moment later, opening.
void onPillClicked()
{
    if (!g_dialog || g_lent) return;
    g_settling = true; // moving to the dialog's monitor dispatches too: not sent home before core has answered
    g_hooks.veil();
    float centre = hangLayers();
    g_hooks.dispatch(PickerHung{ centre }, "file dialog's pill");
    g_settling = false;
    if (picker()) after(tokens().pickerSwap, open);
    else settle(); // core didn't take it (no picking card, hidden): home again, and seen again
}

void onDialog(std::optional<platform::FileDialogSeen> seen)
{
    bool came = seen && !g_dialog, went = !seen && g_dialog;
    g_dialog = seen;
    // Search's card is the picking one before the pill can open it, and until it has closed with the dialog.
    if (came && g_hooks.search) g_hooks.search->setPicking(true);
    if (went) g_hooks.dispatch(PickerClosed{}, "file dialog gone");
    else if (g_lent && picker()) g_hooks.dispatch(PickerMoved{ hangLayers() }, "file dialog moved");
    settle();
    if (went && g_hooks.search) g_hooks.search->setPicking(false);
}

} // namespace

void watchFilePicker(FilePickerHooks hooks)
{
    g_hooks = std::move(hooks);
    platform::watchFileDialogs(onDialog);
}

void filePillClicked() { onPillClicked(); }

bool sayByFileDialog(const std::string& text)
{
    if (!g_dialog) return false;
    platform::sayOnPickerPill(text); // shown when the pill is: now, or once the card has closed
    return true;
}

bool filePickerShortcut()
{
    if (!g_dialog || !g_hooks.state) return false;
    if (!g_lent) onPillClicked();
    else if (picker() && picker()->stage == Stage::Open)
        g_hooks.dispatch(CardCloseRequested{ g_hooks.state().expandedId }, "shortcut"); // again: it closes, as Search's does
    return true;
}

void settleFilePicker() { settle(); }

} // namespace cup::app
