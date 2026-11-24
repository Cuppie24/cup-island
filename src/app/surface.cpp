#include "app/surface.h"

namespace cup::app {

Vec2 toLocal(const Surface& s, Vec2 point) { return { point.x - s.scene.offset.x, point.y - s.scene.offset.y }; }

bool onShape(const Surface& s, Vec2 point)
{
    return Rect{ s.scene.offset.x, s.scene.offset.y, s.scene.size.x, s.scene.size.y }.contains(point);
}

bool onBubble(const Surface& s, Vec2 point) { return s.scene.bubble && s.scene.bubble->frame.contains(point); }

const Placed* findElement(const Surface& s, const std::string& key)
{
    for (const auto& p : s.scene.elements)
        if (p.key == key) return &p;
    if (s.scene.toast) // a floating toast's (decision 071)
        for (const auto& p : s.scene.toast->elements)
            if (p.key == key) return &p;
    return nullptr;
}

// Re-run on every pointer move and every new scene: content can move under a still pointer.
const Placed* contentUnder(const Surface& s, Vec2 point)
{
    if (s.scene.toast && s.scene.toast->frame.contains(point)) return nullptr; // the toast floats over it (decision 071)
    for (const Placed& p : s.scene.elements)
        if (p.hitFrame.contains(point) && p.key.rfind("tab/", 0) != 0) return &p;
    return nullptr;
}

const Tab* tabUnder(const Surface& s, Vec2 point) { return s.scene.tabs ? tabAt(*s.scene.tabs, point) : nullptr; }

bool isCurrent(const Surface& s, const Tab& tab)
{
    if (!s.scene.tabs) return false;
    const auto& strip = *s.scene.tabs;
    return strip.active && &tab == &strip.tabs[*strip.active];
}

} // namespace cup::app
