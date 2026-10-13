#include "core/split.h"

#include "core/lift.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {

Rect splitPillFrame(const IslandState& s)
{
    const Tokens& t = tokens();
    return { placementOf(s, Mode::Split).pillX, 0, t.compactSize.x, t.compactSize.y };
}

float splitGap()
{
    // Each flare is capped by its shape's height, as drawn (blob_shape.h).
    const Tokens& t = tokens();
    if (t.flareRadius <= 0) return t.bubbleGap;
    return std::min(t.flareRadius, t.compactSize.y) + std::min(t.flareRadius, t.bubbleSize);
}

Rect bubbleHomeFrame(const IslandState& s)
{
    const Tokens& t = tokens();
    Rect pill = splitPillFrame(s);
    return { pill.x + pill.width + splitGap(), pill.y + (pill.height - t.bubbleSize) / 2, t.bubbleSize, t.bubbleSize };
}

bool bubbleOverPill(const IslandState& s, Vec2 drag)
{
    Rect home = bubbleHomeFrame(s);
    Vec2 centre{ home.x + home.width / 2 + drag.x, home.y + home.height / 2 + drag.y };
    return splitPillFrame(s).contains(centre);
}

bool isDrag(Vec2 drag) { return std::hypot(drag.x, drag.y) >= tokens().dragStartDistance; }

} // namespace cup
