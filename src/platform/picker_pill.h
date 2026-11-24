#pragma once
// The pill under a file dialog (decision 181): a small window of its own,
// the size of the idle island, hung from the dialog's bottom edge with a
// search glyph on it. The island stays where it is; a click on this pill
// brings it there with the card that picks files (app/file_picker).
// It never takes the keys from the dialog, and it isn't animated: it moves
// with the dialog, as one piece. Windows 10 1809+ throughout (decision 015).

#include "platform/graphics_device.h"
#include "platform/win.h"

#include <functional>

namespace cup::platform {

// Once, on the UI thread: hidden until showPickerPill(). `compositor` and
// `device` are the island's (decision 104: one of each for every window).
void createPickerPill(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, std::function<void()> onClick);

// Shown at `frame` (screen pixels), drawn in the theme's colours as they are now.
void showPickerPill(Rect frame);
void hidePickerPill();

} // namespace cup::platform
