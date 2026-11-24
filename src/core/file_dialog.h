#pragma once
// A file dialog (Windows' Open or Save) in front of the user, and the island
// by it (decision 180): the island hangs under the dialog, a double-click on
// its pill opens the card that picks files, and what's picked there is given
// to the dialog. Pure: the platform gathers window facts (class names,
// rectangles) and moves the windows; this decides.

#include "core/geometry.h"
#include "core/island.h"

#include <optional>
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

// --- Where the island hangs -------------------------------------------------
struct Hang {
    float top = 0;    // the island's top edge: the dialog's bottom, screen pixels
    float centre = 0; // the pill's centre: the dialog's, screen pixels
    bool operator==(const Hang&) const = default;
};
// Under `dialog`, on the island's monitor (`workArea`: it less the taskbar).
// Nothing when the dialog is on another monitor or has less than
// pickerPillRoom under it: the island stays in its own place. With a card
// open it needs pickerCardRoom, and moves up over the dialog's lower part
// rather than off the screen's bottom.
std::optional<Hang> hangUnder(Rect dialog, Rect workArea, bool cardOpen);

// --- The island's rules while one is in front --------------------------------
// A click on the resting pill (or the bubble) does nothing: a double-click opens it.
bool opensOnDoubleClick(const IslandState& s);
std::string pickingCard(const IslandState& s);                    // ...on this panel's card (Panel::picksFiles); "" = none
bool isPickingCard(const IslandState& s, const std::string& id);  // that card: it closes when the dialog goes

} // namespace cup
