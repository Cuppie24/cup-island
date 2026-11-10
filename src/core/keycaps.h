#pragma once
// Keys as key caps (decision 092): how a key combination's text splits into
// caps and the plain words between them, and how wide each is. Shared by
// layout (the item's width) and the renderer (where each cap goes).

#include "core/layout.h"

#include <string>
#include <vector>

namespace cup {

struct KeyPart {
    std::string label;
    bool cap = true; // a key in a cap; false: a word between keys ("or", "to", "Double")
    bool operator==(const KeyPart&) const = default;
};

// "Ctrl+Shift+Tab" -> Ctrl, Shift, Tab; "Enter or Space" -> Enter, or, Space;
// "1 to 9" -> 1, to, 9; "Double Ctrl" -> Double, Ctrl. A part is a cap
// unless it's one of the joining words.
std::vector<KeyPart> keyParts(const std::string& text);

// The text a part is drawn with: caption size; a cap's label in body colour.
Text keyPartText(const KeyPart& part);

// One part's width: a cap is its label plus keyCapPadding a side, never
// narrower than it is tall; a word is its text.
float keyPartWidth(const KeyPart& part, const MeasureText& measure);

// The whole item's width: its parts, keyCapGap apart.
float keysWidth(const Keys& keys, const MeasureText& measure);

} // namespace cup
