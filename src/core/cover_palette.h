#pragma once
// The island in the playing cover's colours (decision 051). Pure: pixels in,
// colours out. The provider says which colour (Content::tint); the theme's
// recipe (decision 022) says what to make of it.

#include "core/content.h"
#include "core/tokens.h"

#include <optional>
#include <vector>

namespace cup {

// What the island takes from a cover (decisions 051, 054):
// - main: the hue most of its coloured pixels share, weighted by how vivid
//   they are, so a red logo on grey wins over the grey. Near-black,
//   near-white and grey pixels don't count (text, borders);
// - second: another hue at least 45° away with coverSecondShare of the main
//   one's weight (a two-colour cover), else none;
// - vividness: the main colour's OKLCH chroma over coverFullChroma, 0..1.
// nullopt for a cover with too few coloured pixels: the theme stays.
std::optional<CoverTint> coverTint(const ImageData& image);

// The colour tokens `themed` (tokens with the app theme applied) would have
// in a theme built around `tint`: the same recipe and opacities, the cover's
// hues, as strong as the cover is vivid. A dark theme gets a tinted dark
// island, a light one a pale tint; a second hue is the island's right end
// (islandSecondColor), at the same lightness, so text reads the same on both.
std::vector<ThemeColor> coverColors(const CoverTint& tint, const Tokens& themed);

} // namespace cup
