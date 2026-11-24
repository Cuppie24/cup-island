#pragma once
// A file dialog (Windows' Open or Save) in front of the user, and the island
// by it (decision 181): a pill of its own sits under the dialog; clicked, the
// island comes there with the card that picks files, and what's picked is
// given to the dialog. Pure: the platform gathers window facts (class names,
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
// Under `dialog`. The card (`cardOpen`) needs pickerCardRoom above the
// bottom of `workArea` (the dialog's monitor less its taskbar), and moves up
// over the dialog's lower part to have it.
Hang hangUnder(Rect dialog, Rect workArea, bool cardOpen);

// The pill by the dialog: `size` big, centred on it.
struct PillPlace {
    Rect frame;         // screen pixels, on whole ones
    bool above = false; // standing on the dialog's top edge, not hanging from its bottom one
    bool operator==(const PillPlace&) const = default;
};
// Hung from the dialog's bottom edge. With no room for it between there and
// the bottom of `workArea`, it stands on the dialog's top edge instead, its
// flat side and flared corners down; with no room there either (a dialog as
// tall as the screen), it's under the dialog after all, cut off.
PillPlace pillPlace(Rect dialog, Rect workArea, Vec2 size);

// --- The picking card ---------------------------------------------------------
std::string pickingCard(const IslandState& s);                    // the panel whose card picks files (Panel::picksFiles); "" = none
bool isPickingCard(const IslandState& s, const std::string& id);  // that card: it closes when the dialog goes

} // namespace cup
