#pragma once
// Notices when a file dialog (Windows' Open or Save) of another app is in
// front, follows it as it moves, and gives it a path (decision 181). Windows
// has no event for it: a WinEvent hook on foreground changes finds a dialog
// with Explorer's file view in it (core/file_dialog.h decides which window
// is one). Event-driven, no polling. Every API here is Windows Vista or
// older (decision 015).

#include "platform/win.h"

#include <functional>
#include <optional>
#include <string>

namespace cup::platform {

struct FileDialogSeen {
    HWND hwnd{};
    Rect bounds; // its visible frame, screen pixels
    bool operator==(const FileDialogSeen&) const = default;
};

// `onChange`: one came in front or moved, or (nothing) it closed or another
// app's window is in front. The island's own windows in front change
// nothing: its card takes the keys while the dialog waits. Called on this
// (the UI) thread, only when something changed.
void watchFileDialogs(std::function<void(std::optional<FileDialogSeen>)> onChange);

// Gives `path` (UTF-8) to the dialog being watched: put in its "File name"
// field and confirmed, as if typed and Enter pressed. A file is chosen (or
// saved over, after the dialog's own question); a folder is gone to. False
// if there's no dialog or it didn't take the text (an elevated app's
// doesn't, from an island that isn't): that dialog is brought in front all
// the same, so the caller can leave the path on the clipboard for it.
bool giveToFileDialog(const std::string& path);

} // namespace cup::platform
