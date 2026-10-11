#include "core/split.h"

#include "core/tokens.h"

#include <cmath>

namespace cup {

Rect splitPillFrame()
{
    const Tokens& t = tokens();
    return { (t.canvas.x - t.compactSize.x) / 2, 0, t.compactSize.x, t.compactSize.y };
}

Rect bubbleHomeFrame()
{
    const Tokens& t = tokens();
    Rect pill = splitPillFrame();
    return { pill.x + pill.width + t.bubbleGap, pill.y + (pill.height - t.bubbleSize) / 2, t.bubbleSize, t.bubbleSize };
}

bool bubbleOverPill(Vec2 drag)
{
    Rect home = bubbleHomeFrame();
    Vec2 centre{ home.x + home.width / 2 + drag.x, home.y + home.height / 2 + drag.y };
    return splitPillFrame().contains(centre);
}

bool isDrag(Vec2 drag) { return std::hypot(drag.x, drag.y) >= tokens().dragStartDistance; }

} // namespace cup
