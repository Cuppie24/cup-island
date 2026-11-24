#pragma once
// The app's side of a file dialog in front (decision 181): the dialog has an
// island of its own (main.cpp builds it), and this keeps it by the dialog:
// shown while one is in front, hung where core says (core/file_dialog.h:
// placeByDialog), moved with the dialog, hidden when it goes. The rules are
// core's; this makes the calls.

#include "core/island.h"

#include <functional>

namespace cup::app {

// What it needs from the app, which owns both islands (main.cpp).
struct FilePickerHooks {
    // Runs `work` on the dialog's island: `state`, `dispatch` and the calls
    // below mean that island inside it, and only there.
    std::function<void(const std::function<void()>& work)> onIsland;
    std::function<const IslandState&()> state;
    std::function<void(const Event&, const char* reason)> dispatch;
    std::function<Rect(Rect dialog)> workAreaOf;  // the dialog's monitor less its taskbar, screen pixels
    std::function<void(Vec2 origin)> moveTo;      // its layers' top left corner there (screen pixels), at once
    std::function<void(bool visible)> setVisible; // its windows shown (without taking the keys) or hidden
    std::function<void(bool in)> fade;            // ...fading in from nothing, or out, over pickerFade
    std::function<void()> focus;                  // it takes the keys: its card's field is typed in
    std::function<bool()> hasKeys;                // it has them now
};

// Starts watching for file dialogs. After the dialog's island is built.
void watchFilePicker(FilePickerHooks hooks);

// After every dispatch on the dialog's island (called on it): its card
// opening or closing changes where it hangs, and closed, the dialog has the keys again.
void settleFilePicker();

// Its layer changed size (new tokens, called on the dialog's island): put by the dialog again.
void refitFilePicker();

// Search's shortcut was pressed. With a file dialog in front it opens the
// dialog's island's card, and pressed again closes it; true then. False: no
// dialog, the shortcut is Search's own.
bool filePickerShortcut();

} // namespace cup::app
