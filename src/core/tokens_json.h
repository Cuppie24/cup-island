#pragma once
// Reads and writes tokens.json: a flat JSON object of token name -> number,
// or [x, y] for sizes, or [x1, y1, x2, y2] for easing curves. Durations are
// in milliseconds and their names end in "Ms". Pure: text in, text out.

#include "core/tokens.h"

#include <string>
#include <string_view>
#include <vector>

namespace cup {

struct TokensParse {
    Tokens tokens;                     // defaults, overridden by every key in the file
    std::vector<std::string> problems; // "line 4: ..." messages; any problem = don't apply
    bool ok() const { return problems.empty(); }
};

// Missing keys keep their defaults. Unknown keys, wrong shapes, out-of-range
// values and syntax errors are all reported, so a typo never silently does nothing.
TokensParse parseTokens(std::string_view json);

// Every token with its current value, one per line.
std::string toJson(const Tokens& tokens);

// tokens.json is in logical pixels (100% display scaling). The app multiplies
// every length (sizes, gaps, paddings, fonts, radii, speeds in px/s) by the
// display's scale, so the island is as big on a 150% screen as Windows'
// own UI. Durations, counts, curves, colours and pure factors stay as they are.
Tokens scaleTokens(const Tokens& tokens, float scale);

} // namespace cup
