#pragma once
// Places one slot's rows of items inside a frame: sizes, alignment, edge
// groups and click areas (PS §5.1). Shared by the pill, the bubble, and
// Home's tiles (decision 009).

#include "core/layout.h"

#include <map>
#include <string>
#include <vector>

namespace cup {

// Whose content is being placed: stamped on every Placed, so clicks route back.
struct Source {
    const Content* content = nullptr;
    std::string providerId;
    std::string activityId;
    const Draft* draft = nullptr; // text being typed: fills a matching TextInput (decision 016)
    const std::map<std::string, ListScroll>* scrolls = nullptr; // how its lists are scrolled (decision 029)
    const ListDrag* listDrag = nullptr;                          // a list row being dragged
    const std::map<std::string, std::string>* keptText = nullptr; // fields' unsent text while unfocused (decision 050)
};

struct SlotFrame {
    std::string name;    // key prefix: "<name>/<row>/<item>"
    const Slot* slot;
    Rect frame;          // pill (or bubble) coordinates
    bool square = false; // a one-glyph square (compact ends, an alert's leading, the bubble): content always centred in it
};

// Appends the slot's items to `out`, rows stacked and centred vertically in the frame.
// Rounded items (art, button hover) get `nestedRadius`, capped at half their size.
// `measure` sizes large time labels to their digits (estimated without it).
void placeSlot(const SlotFrame& slot, const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out,
               const MeasureText& measure = {});

// The height a slot's rows need, stacked with their spacing: how tall a
// Centred card grows (decision 016). Wrapping text needs `measure` (decision
// 045), and text being edited the `source`'s draft.
float slotHeight(const Slot& slot, Rect frame, const Source& source = {}, const MeasureText& measure = {});

// Font size in pixels for a text style (tokens).
float fontSize(TextStyle style);

// Without the real font: an average glyph is about half the font size wide (tests).
float estimateTextWidth(const Text& text);

} // namespace cup
