#include "core/badges.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// In an area's top-right corner, concentric with its rounding (decision 006):
// at the centre of the corner's arc, so the gap to the curved edge is the
// same all the way round. A corner tighter than the dot plus badgeInset
// (square, or nearly) keeps that much in from both edges instead.
Vec2 inCorner(const Rect& r, float cornerRadius)
{
    const Tokens& t = tokens();
    float centre = std::max(cornerRadius, t.badgeSize / 2 + t.badgeInset);
    return { r.x + r.width - centre, r.y + centre };
}

} // namespace

std::vector<Vec2> badgeCentres(const Scene& scene, bool accent)
{
    std::vector<Vec2> dots;
    for (const Tile& tile : scene.tiles)
        if (tile.badge && tile.accent == accent) dots.push_back(inCorner(tile.frame, scene.tileRadius));
    if (scene.tabs) { // a page's dot: the badge just off its top-right (decision 109); Home's button: in its corner
        for (const Tab& tab : scene.tabs->tabs)
            if (tab.badge && tab.accent == accent) dots.push_back({ tab.frame.x + tab.frame.width, tab.frame.y });
        if (scene.tabs->home && scene.tabs->home->badge && scene.tabs->home->accent == accent) dots.push_back(inCorner(scene.tabs->home->frame, scene.tabs->radius));
        for (const Tab& button : scene.tabs->buttons) // a panel's button (decision 113): as Home's
            if (button.badge && button.accent == accent) dots.push_back(inCorner(button.frame, scene.tabs->radius));
    }
    if (accent) return dots; // a button on a card: always red
    for (const Placed& p : scene.elements) {
        auto* button = std::get_if<Button>(&p.item);
        if (!button || !button->badge) continue;
        // On the button's round outline (its hover shape) at its top-right,
        // 45° up from its centre: on the circle, not off in its square's corner.
        float round = std::min(p.frame.width, p.frame.height) / 2;
        float out = round * 0.7071f; // cos 45°
        Vec2 at{ p.frame.x + p.frame.width / 2 + out, p.frame.y + p.frame.height / 2 - out };
        if (p.list && !p.list->viewport.contains(at)) continue; // scrolled out of view
        dots.push_back(at);
    }
    return dots;
}

} // namespace cup
