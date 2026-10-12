#include "core/keyboard.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cup {
namespace {

Vec2 centre(const Rect& r) { return { r.x + r.width / 2, r.y + r.height / 2 }; }
bool empty(const Rect& r) { return r.width <= 0 || r.height <= 0; }

// What a click on this element does something with: the same items hitTest answers for.
bool clickable(const Item& item)
{
    if (auto* button = std::get_if<Button>(&item)) return button->enabled;
    if (auto* image = std::get_if<Image>(&item)) return !image->actionId.empty();
    return std::holds_alternative<TextInput>(item);
}

// How far apart two ranges are along one axis: 0 when they overlap.
float gap(float a0, float a1, float b0, float b1) { return std::max({ 0.0f, b0 - a1, a0 - b1 }); }

} // namespace

std::vector<Focusable> focusables(const Scene& scene)
{
    std::vector<Focusable> found;
    for (const Placed& p : scene.elements) {
        if (!clickable(p.item)) continue;
        // A whole-row button (Row::clickAnywhere) is aimed at by its row, which is what lights up.
        Rect frame = p.listRow ? *p.listRow : p.frame;
        if (p.list) frame = p.frame.height > 0 ? Rect{ frame.x, p.frame.y, frame.width, p.frame.height } : frame; // where the row is, scrolled
        Vec2 point = empty(p.hitFrame) ? centre(p.frame) : centre(p.hitFrame);
        found.push_back({ "el:" + p.key, frame, point, p.list, p.activityId });
    }
    for (const Tile& tile : scene.tiles)
        if (tile.clickable()) found.push_back({ "tile:" + tile.id, tile.frame, centre(tile.frame), std::nullopt, tile.id });
    if (scene.tabs)
        for (std::size_t i = 0; i < scene.tabs->tabs.size(); ++i) {
            const Tab& tab = scene.tabs->tabs[i];
            if (scene.tabs->active == i) continue; // shown already: nothing to switch to
            found.push_back({ "tab:" + tab.id, tab.frame, centre(tab.hitFrame), std::nullopt, tab.id });
        }
    std::stable_sort(found.begin(), found.end(), [](const Focusable& a, const Focusable& b) {
        float ay = centre(a.frame).y, by = centre(b.frame).y;
        if (std::abs(ay - by) > 1) return ay < by;
        return a.frame.x < b.frame.x;
    });
    return found;
}

std::optional<Focusable> findFocusable(const Scene& scene, const std::string& id)
{
    for (Focusable& f : focusables(scene))
        if (f.id == id) return f;
    return std::nullopt;
}

std::optional<Focusable> moveFocus(const Scene& scene, const std::string& current, Arrow arrow)
{
    std::vector<Focusable> all = focusables(scene);
    if (all.empty()) return std::nullopt;
    auto from = std::find_if(all.begin(), all.end(), [&](const Focusable& f) { return f.id == current; });
    if (from == all.end()) return all.front();

    const Rect a = from->frame;
    Vec2 c = centre(a);
    bool vertical = arrow == Arrow::Up || arrow == Arrow::Down;
    float sign = arrow == Arrow::Down || arrow == Arrow::Right ? 1.0f : -1.0f;
    std::optional<Focusable> best;
    float bestScore = std::numeric_limits<float>::max();
    for (const Focusable& f : all) {
        if (f.id == current) continue;
        const Rect& b = f.frame;
        Vec2 d = centre(b);
        float along = sign * (vertical ? d.y - c.y : d.x - c.x);
        if (along <= 1) continue; // not that way (a pixel of slack: rows of one height line up)
        // Left and right stay on the row: a button beside, never the row above.
        // Up and down prefer what's in line, then the nearest off to the side
        // (from a tab up to the buttons above it), off to the side counting double.
        float aside = vertical ? gap(a.x, a.x + a.width, b.x, b.x + b.width) : gap(a.y, a.y + a.height, b.y, b.y + b.height);
        if (!vertical && aside > 0) continue;
        float score = along + 2 * aside + (aside > 0 ? 10000.0f : 0.0f);
        if (score < bestScore) {
            bestScore = score;
            best = f;
        }
    }
    return best;
}

} // namespace cup
