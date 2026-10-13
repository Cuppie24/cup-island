#include "core/badges.h"

#include "core/tokens.h"

namespace cup {
namespace {

// Inside an area's top-right corner: a widget's or tab's rounded background.
Vec2 inCorner(const Rect& r)
{
    const Tokens& t = tokens();
    float radius = t.badgeSize / 2;
    return { r.x + r.width - t.badgeInset - radius, r.y + t.badgeInset + radius };
}

} // namespace

std::vector<Vec2> badgeCentres(const Scene& scene)
{
    std::vector<Vec2> dots;
    for (const Tile& tile : scene.tiles)
        if (tile.badge) dots.push_back(inCorner(tile.frame));
    if (scene.tabs)
        for (const Tab& tab : scene.tabs->tabs)
            if (tab.badge) dots.push_back(inCorner(tab.frame));
    float radius = tokens().badgeSize / 2;
    for (const Placed& p : scene.elements) {
        auto* button = std::get_if<Button>(&p.item);
        if (!button || !button->badge) continue;
        // On the button's corner, overlapping its glyph's square a little, like an app icon's.
        Vec2 at{ p.frame.x + p.frame.width - radius, p.frame.y + radius };
        if (p.list && !p.list->viewport.contains(at)) continue; // scrolled out of view
        dots.push_back(at);
    }
    return dots;
}

} // namespace cup
