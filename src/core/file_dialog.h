#pragma once
// A file dialog (Windows' Open or Save) in front of the user, and the island
// of its own that hangs from it (decision 181): a second island, with one
// card, Search picking a file for the dialog. Pure: the platform gathers
// window facts (class names, rectangles) and moves the windows; this decides.

#include "core/geometry.h"

#include <string>
#include <vector>

namespace cup {

// --- Which window is one ----------------------------------------------------
// `windowClass`: the foreground window's own; `inside`: its descendants'.
// Windows has no event for "a file dialog opened", but both kinds are a
// dialog ("#32770") with Explorer's file view in it.
bool isFileDialog(const std::string& windowClass, const std::vector<std::string>& inside);
// A dialog that isn't one yet: old-style dialogs build their file view after
// they're shown, so the platform looks once more, pickerRecheck later.
bool mayBecomeFileDialog(const std::string& windowClass, const std::vector<std::string>& inside);
// `chain`: the classes from the dialog's child down to a control. True for
// its "File name" field: an Edit that isn't the address bar's or search's.
bool isFileNameField(const std::vector<std::string>& chain);

// --- Where its island hangs --------------------------------------------------
// The island's render layer there: `canvas` big, the island centred along its
// top edge, as on a screen.
struct DialogPlace {
    Vec2 origin;        // the layer's top left corner, screen pixels, on whole ones
    bool above = false; // over the dialog's top edge instead of under its bottom one
    bool operator==(const DialogPlace&) const = default;
};
// `dialog`: its frame; `workArea`: its monitor less the taskbar; `pillHeight`: the resting island's.
//   - Centred on the dialog, and kept on the screen sideways.
//   - Resting, it hangs from the dialog's bottom edge. With no room for the
//     pill between there and the bottom of the work area, it sits over the
//     dialog's top edge instead; with none there either (a dialog as tall as
//     the screen), under it after all, cut off.
//   - With a card open it's always under, and needs pickerCardRoom above the
//     bottom of the work area: it moves up over the dialog's lower part to have it.
DialogPlace placeByDialog(Rect dialog, Rect workArea, Vec2 canvas, float pillHeight, bool cardOpen);

} // namespace cup
