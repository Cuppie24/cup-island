#pragma once
// The app's side of a file dialog in front (decision 181): the watcher's
// news becomes core's PickerOpened / PickerClosed, the island's layers hang
// under the dialog where core says (core/file_dialog.h: hangUnder), and
// Search's card picks the file. The rules are core's; this makes the calls.

#include "core/island.h"

#include <functional>
#include <optional>

namespace cup {
class SearchProvider;
}

namespace cup::app {

// What it needs from the app, which owns the state and the layers (main.cpp).
struct FilePickerHooks {
    std::function<const IslandState&()> state;
    std::function<void(const Event&, const char* reason)> dispatch;
    std::function<Rect()> workArea;   // the island's monitor less its taskbar, screen pixels
    std::function<float()> layerLeft; // the render layer's left edge, screen pixels
    // The island's layers hang from this y (screen pixels); none: back at their monitor's top.
    std::function<void(std::optional<float> top)> hang;
    SearchProvider* search = nullptr; // its card picks the file (SearchProvider::setPicking)
};

// Starts watching. Before the message loop runs, after the providers started.
void watchFilePicker(FilePickerHooks hooks);

// After every dispatch, and when the island's monitor changed: a card opening
// or closing, or another work area, changes where the island hangs.
void settleFilePicker();

} // namespace cup::app
