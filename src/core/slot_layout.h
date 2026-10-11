#pragma once
// Places one slot's rows of items inside a frame: sizes, alignment, edge
// groups and click areas (PS §5.1). Shared by the pill, the bubble, and
// Home's tiles (decision 009).

#include "core/layout.h"

#include <string>
#include <vector>

namespace cup {

// Whose content is being placed: stamped on every Placed, so clicks route back.
struct Source {
    const Content* content = nullptr;
    std::string providerId;
    std::string activityId;
};

struct SlotFrame {
    std::string name;    // key prefix: "<name>/<row>/<item>"
    const Slot* slot;
    Rect frame;          // pill (or bubble) coordinates
    bool square = false; // a one-glyph square (compact ends, the bubble): content always centred in it
};

// Appends the slot's items to `out`, rows stacked and centred vertically in the frame.
// Rounded items (art, button hover) get `nestedRadius`, capped at half their size.
void placeSlot(const SlotFrame& slot, const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out);

// Font size in pixels for a text style (tokens).
float fontSize(TextStyle style);

} // namespace cup
