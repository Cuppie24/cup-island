#pragma once
// Glyphs drawn as lines instead of taken from the icon font (decision 174):
// a processor, a graphics card, memory and the network's two arrows. Segoe
// MDL2 Assets (all Windows 10 has, decision 015) has none of the first
// three, so they're the design's own shapes, the same on every Windows.

#include "core/content.h"
#include "platform/graphics_device.h"

namespace cup::platform {

// Draws `glyph` `fontSize` big, centred in a surface of `size`, if it's one
// of the drawn ones; false (nothing drawn) for a font's glyph.
bool paintDrawnGlyph(ID2D1DeviceContext* dc, Glyph glyph, float fontSize, Vec2 size, const Color& color);

} // namespace cup::platform
