#pragma once
// The island in the playing cover's colours (decision 051). Pure: pixels in,
// colours out. The provider says which colour (Content::tint); the theme's
// recipe (decision 022) says what to make of it.

#include "core/content.h"
#include "core/tokens.h"

#include <optional>
#include <vector>

namespace cup {

// The cover's main colour: the hue most of its coloured pixels share,
// weighted by how vivid they are, so a red logo on grey wins over the grey.
// Near-black, near-white and grey pixels don't count (text, borders).
// nullopt for a cover with too few coloured pixels: the theme stays.
std::optional<Color> coverColor(const ImageData& image);

// The colour tokens `themed` (tokens with the app theme applied) would have
// in a theme built around `cover`: the same recipe and opacities, the cover's
// hue. A dark theme gets a tinted near-black island, a light one a pale tint.
std::vector<ThemeColor> coverColors(Color cover, const Tokens& themed);

} // namespace cup
