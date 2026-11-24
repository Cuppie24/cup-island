#pragma once
// The pill by a file dialog (decision 181): a small window of its own, the
// size of the idle island, hung from the dialog's bottom edge (or standing
// on its top one) with a search glyph on it. The island stays where it is; a
// click on this pill brings it there with the card that picks files
// (app/file_picker). It can carry a short note instead of its glyph, growing
// to fit it. It never takes the keys from the dialog. It fades in and out
// over pickerFade, and moves with the dialog at once, as one piece.
// Windows 10 1809+ throughout (decision 015).

#include "platform/graphics_device.h"
#include "platform/win.h"

#include <functional>
#include <string>

namespace cup::platform {

// Once, on the UI thread: hidden until showPickerPill(). `compositor` and
// `device` are the island's (decision 104: one of each for every window).
void createPickerPill(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, std::function<void()> onClick);

// Shown at `frame` (screen pixels; core/file_dialog.h: pillPlace), drawn in
// the theme's colours as they are now, on top of every window. Its flat
// side, with the island's flared corners, is the one on the dialog: its top,
// or with `above` its bottom. Called again while shown, it moves.
// `atOnce`: no fade, when the island takes its place or gives it back.
void showPickerPill(Rect frame, bool above, bool atOnce = false);
void hidePickerPill(bool atOnce = false); // fades out, then its window hides

// `text` on the pill in place of its glyph, for pickerNote from now: the pill
// is as wide as it needs. Said while hidden, it's there when the pill shows.
void sayOnPickerPill(const std::string& text);

} // namespace cup::platform
