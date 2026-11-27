#pragma once
// Pixels for the measurement tiles (core/stat_items.h, decision 174): a
// reading with its level, and devices' batteries. Each is painted whole into
// its surface, like a Dial's middle: they change every couple of seconds,
// not every frame, so a new reading is a repaint, not an animation. Every
// part's place comes from core/stat_parts.h; nothing here decides geometry.

#include "core/content.h"
#include "platform/graphics_device.h"

namespace cup::platform {

// Paints `item` if it's one of the measurement tiles, and says whether it was.
// `size`: its surface's, in pixels. `radius`: its corners
// (Placed::cornerRadius). `live`: the colour of a level with nothing wrong
// (the content's accent, else liveColor); a reading that is high or low is
// drawn in dangerColor instead (its own `alert`).
bool paintStat(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Item& item, Vec2 size, float radius, const Color& live);

} // namespace cup::platform
