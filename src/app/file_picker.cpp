#include "app/file_picker.h"

#include "core/file_dialog.h"
#include "core/lift.h"
#include "core/tokens.h"
#include "platform/file_dialog_watcher.h"
#include "platform/picker_pill.h"
#include "providers/search/search_provider.h"

#include <utility>

namespace cup::app {
namespace {

FilePickerHooks g_hooks;
std::optional<platform::FileDialogSeen> g_dialog; // the one in front
bool g_lent = false;                              // the island's layers hang under it
bool g_settling = false;                          // settle() dispatches, and every dispatch ends in it

bool lent() { return g_hooks.state().picker.has_value(); } // core's word: the picking card is open under the dialog

Hang hangFor(bool cardOpen, std::string* displayId = nullptr)
{
    PickerDisplay display = g_hooks.displayOf(g_dialog->bounds);
    if (displayId) *displayId = display.id;
    return hangUnder(g_dialog->bounds, display.workArea, cardOpen);
}

// The pill: under the dialog while one is in front and the island isn't there instead.
void placePill()
{
    if (g_dialog && !g_lent) platform::showPickerPill(pillFrame(hangFor(false), modeSize(Mode::Idle)));
    else platform::hidePickerPill();
}

// The layers under the dialog, then core told where the card's middle is in them.
float hangLayers()
{
    std::string displayId;
    Hang hang = hangFor(true, &displayId);
    g_lent = true;
    g_hooks.hang(HangPlace{ displayId, hang.top });
    return hang.centre - g_hooks.layerLeft();
}

void sendHome()
{
    if (!std::exchange(g_lent, false)) return;
    g_hooks.hang(std::nullopt);
}

void settle()
{
    if (g_settling || !g_hooks.state) return;
    g_settling = true;
    if (g_lent && !lent()) sendHome(); // the card closed, any way it can
    placePill();
    g_settling = false;
}

// The pill was clicked: the layers first, so the card opens where it will be.
void onPillClicked()
{
    if (!g_dialog || lent()) return;
    g_settling = true; // moving to the dialog's monitor dispatches too: not sent home before core has answered
    float centre = hangLayers();
    g_hooks.dispatch(PickerOpened{ centre }, "file dialog's pill");
    g_settling = false;
    settle(); // home again if core didn't open it (no picking card, hidden)
    if (lent()) g_hooks.focus();
}

void onDialog(std::optional<platform::FileDialogSeen> seen)
{
    bool came = seen && !g_dialog, went = !seen && g_dialog;
    g_dialog = seen;
    // Search's card is the picking one before the pill can open it, and until it has closed with the dialog.
    if (came && g_hooks.search) g_hooks.search->setPicking(true);
    if (went) g_hooks.dispatch(PickerClosed{}, "file dialog gone");
    else if (lent()) g_hooks.dispatch(PickerMoved{ hangLayers() }, "file dialog moved");
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

void settleFilePicker() { settle(); }

} // namespace cup::app
