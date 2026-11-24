#include "app/file_picker.h"

#include "core/file_dialog.h"
#include "platform/file_dialog_watcher.h"
#include "providers/search/search_provider.h"

#include <utility>

namespace cup::app {
namespace {

FilePickerHooks g_hooks;
std::optional<platform::FileDialogSeen> g_dialog; // the one in front
std::optional<float> g_top;                       // where the layers hang now; none: their monitor's top
bool g_settling = false;                          // settle() dispatches, and every dispatch ends in it

void settle()
{
    if (g_settling || !g_hooks.state) return;
    g_settling = true;
    // The layers first: the pill's place below is measured in them.
    std::optional<Hang> hang;
    if (g_dialog) hang = hangUnder(g_dialog->bounds, g_hooks.workArea(), isExpanded(g_hooks.state()));
    std::optional<float> top = hang ? std::optional<float>(hang->top) : std::nullopt;
    if (top != g_top) {
        g_top = top;
        g_hooks.hang(top);
    }
    const std::optional<Picker> was = g_hooks.state().picker;
    if (!g_dialog) {
        if (was) g_hooks.dispatch(PickerClosed{}, "file dialog gone");
    } else {
        std::optional<float> centre = hang ? std::optional<float>(hang->centre - g_hooks.layerLeft()) : std::nullopt;
        if (!was || was->centre != centre) g_hooks.dispatch(PickerOpened{ centre }, was ? "file dialog moved" : "file dialog in front");
    }
    g_settling = false;
}

void onDialog(std::optional<platform::FileDialogSeen> seen)
{
    bool came = seen && !g_dialog, went = !seen && g_dialog;
    g_dialog = seen;
    // Search's card is the picking one before the island says a dialog is
    // there, and until its card has closed with the dialog.
    if (came && g_hooks.search) g_hooks.search->setPicking(true);
    settle();
    if (went && g_hooks.search) g_hooks.search->setPicking(false);
}

} // namespace

void watchFilePicker(FilePickerHooks hooks)
{
    g_hooks = std::move(hooks);
    platform::watchFileDialogs(onDialog);
}

void settleFilePicker() { settle(); }

} // namespace cup::app
