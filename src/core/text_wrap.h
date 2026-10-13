#pragma once
// Breaking a text into lines (decision 045). Core decides where the lines
// break, with the platform's measurements, and hands the painter the text
// with those spaces turned into '\n': what's drawn is exactly what was laid
// out, and offsets into the text still hold (a caret, a selection).

#include "core/layout.h"

#include <string>

namespace cup {

// `text` with the spaces where its lines break, `width` wide, turned into
// '\n'. Breaks only at spaces: a word longer than a line keeps its own line
// (and ends in "…"). Measured word by word: each width is rounded up, so a
// line never comes out wider than it was measured.
std::string wrapText(const std::string& text, TextStyle style, float width, const MeasureText& measure);

int lineCount(const std::string& wrapped); // its '\n's plus one

} // namespace cup
