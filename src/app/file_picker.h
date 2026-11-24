#pragma once
// The app's side of a file dialog in front (decision 181): a pill of its own
// under the dialog (platform/picker_pill); clicked, the island's layers are
// hung under the dialog and core opens the picking card there
// (PickerOpened); when that card closes the layers go home and the pill is
// back. Search is told while a dialog is in front, so its card picks.
// The rules are core's (core/file_dialog.h); this makes the calls.

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
    // The island was made invisible for its jump between its own place and
    // the dialog (main's dispatch does that when IslandState::picker comes
    // or goes): now it's there, it fades in.
    std::function<void()> unveil;
    std::function<void()> veil;       // ...and invisible before the layers move to the dialog
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

// After every dispatch: the picking card closed, so the island goes home and the pill is back.
void settleFilePicker();

} // namespace cup::app
