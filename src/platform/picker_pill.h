#pragma once
// The pill under a file dialog (decision 181): a small window of its own,
// the size of the idle island, hung from the dialog's bottom edge with a
// search glyph on it. The island stays where it is; a click on this pill
// brings it there with the card that picks files (app/file_picker).
// It never takes the keys from the dialog. It fades in and out over
// pickerFade, and moves with the dialog at once, as one piece. Windows 10 1809+ throughout (decision 015).

#include "platform/graphics_device.h"
#include "platform/win.h"

#include <functional>

namespace cup::platform {

// Once, on the UI thread: hidden until showPickerPill(). `compositor` and
// `device` are the island's (decision 104: one of each for every window).
void createPickerPill(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, std::function<void()> onClick);

// Shown at `frame` (screen pixels), drawn in the theme's colours as they are
// now, and raised over the other ordinary windows (never over the taskbar).
// Called again while shown, it moves and is raised again.
void showPickerPill(Rect frame);
void hidePickerPill(); // fades out, then its window hides

} // namespace cup::platform
