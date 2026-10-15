#pragma once
// Reads and writes tokens.json: a flat JSON object of token name -> number,
// or [x, y] for sizes, or [x1, y1, x2, y2] for easing curves. Durations are
// in milliseconds and their names end in "Ms". One nested object, "themes":
// theme name -> { colour token -> colour } (decision 022). Pure: text in, text out.

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
// display's scale times `islandScale`, so the island follows Windows' own
// UI, then the size knob. Durations, counts, curves, colours and pure
// factors stay as they are.
Tokens scaleTokens(const Tokens& tokens, float displayScale);

// The tokens with the named theme's colours applied (decision 022). An
// unknown name changes nothing: the colours as written.
Tokens withTheme(const Tokens& tokens, const std::string& name);

// The tokens with these colours over them: a theme's, or the playing
// cover's palette (core/cover_palette.h, decision 051). Unknown names are skipped.
Tokens withColors(const Tokens& tokens, const std::vector<ThemeColor>& colors);

// True if `a` and `b` differ in colours only (or not at all): a theme pick,
// a cover's tint, or the colour fade's speed. The renderer then fades its shapes to the new colours
// instead of rebuilding them.
bool onlyColorsDiffer(const Tokens& a, const Tokens& b);

} // namespace cup
