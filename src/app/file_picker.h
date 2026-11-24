#pragma once
// The app's side of a file dialog in front (decision 181): a pill of its own
// by the dialog (platform/picker_pill); clicked, the island is lent to the
// dialog and opens the picking card where the pill was; when that card has
// closed back to a pill there, the island goes home and the dialog's pill is
// back. Search is told while a dialog is in front, so its card picks.
// The rules are core's (core/file_dialog.h, Picker); this makes the calls.
//
// The island is never watched travelling between the top of the screen and
// the dialog (its layers jump): it's made invisible, put there in one step,
// and shown the moment it takes the pill's place.

#include "core/island.h"

#include <functional>
#include <optional>
#include <string>

namespace cup {
class SearchProvider;
}

namespace cup::app {

// Where the island's layers hang while lent to a dialog.
struct HangPlace {
    std::string displayId; // the dialog's monitor
    float top = 0;         // their top edge, screen pixels
};

// The monitor a dialog is on.
struct PickerDisplay {
    std::string id;
    Rect workArea; // it less its taskbar, screen pixels
};

// What it needs from the app, which owns the state and the layers (main.cpp).
struct FilePickerHooks {
    std::function<const IslandState&()> state;
    std::function<void(const Event&, const char* reason)> dispatch;
    std::function<PickerDisplay(Rect dialog)> displayOf;
    // The island's layers hang there; none: back at their own monitor's top.
    std::function<void(std::optional<HangPlace>)> hang;
    std::function<float()> layerLeft; // the render layer's left edge, screen pixels: after hang(), the dialog's monitor's
    std::function<void()> focus;      // the island takes the keys: the card's field is typed in at once
    std::function<void()> veil;       // the island invisible, at once
    std::function<void(bool fade)> unveil; // ...and seen again: at once, or fading in over pickerFade
    std::function<Ms()> morphTime;    // how long the morph just started runs
    SearchProvider* search = nullptr; // its card picks the file (SearchProvider::setPicking)
};

// Makes the pill and starts watching. Before the message loop runs, after the providers started.
void watchFilePicker(FilePickerHooks hooks);

// The pill was clicked (platform::createPickerPill's onClick).
void filePillClicked();

// Search's shortcut was pressed. With a file dialog in front it does what the
// pill does, and pressed again closes that card; true then. False: no dialog,
// the shortcut is Search's own.
bool filePickerShortcut();

// A note for the user by the dialog, on its pill (SearchHooks::sayByDialog).
// False with no dialog in front.
bool sayByFileDialog(const std::string& text);

// After every dispatch: the picking card closed, so the island leaves and the pill is back.
void settleFilePicker();

} // namespace cup::app
