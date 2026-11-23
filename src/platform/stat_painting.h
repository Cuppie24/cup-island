#pragma once
// Pixels for the measurement items (core/stat_items.h, decision 172): a
// history graph, a strip of levels, a battery, the overview's two tiles and
// a bar with a plain level. Each is painted whole into its surface, like a
// Dial's middle: they change every couple of seconds, not every frame, so a
// new reading is a repaint, not an animation. Every part's place comes from
// core/stat_parts.h; nothing here decides geometry.

#include "core/content.h"
#include "platform/graphics_device.h"

namespace cup::platform {

// A Progress bar that is painted here rather than built of shapes that
// follow a timeline (element_parts): one with a level.
bool paintedLevel(const Item& item);

// Paints `item` if it's one of the measurement items, and says whether it was.
// `size`: its surface's, in pixels. `radius`: a tile's corners
// (Placed::cornerRadius). `live`: the colour of fills and lines with nothing
// wrong (the content's accent, else liveColor); a reading that is high or
// low is drawn in dangerColor instead (its own `alert`).
bool paintStat(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Item& item, Vec2 size, float radius, const Color& live);

} // namespace cup::platform
