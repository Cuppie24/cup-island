#pragma once
// Notification badges on the island (decision 038): a red dot at each of the
// scene's badge centres (core/badges.h), over the content. Dots fade in and
// out; they're made on first use and kept, like the tile marks.

#include "core/content.h"
#include "core/geometry.h"
#include "platform/win.h"

#include <optional>
#include <vector>

namespace cup::platform {

class BadgeDots {
public:
    // Dots are added to `parent`, above `below` (the content), so content
    // added later still lies under them.
    // `accent`: its dots in the theme's accent (liveColor), not badgeColor (decision 152).
    void build(wuc::Compositor const& compositor, wuc::ContainerVisual const& parent, wuc::Visual const& below, bool accent = false);
    void show(const std::vector<Vec2>& centres); // pill coordinates; the rest fade out
    void recolor(); // tokens changed: every dot fades to badgeColor, or the accent (decision 051)

private:
    struct Dot {
        wuc::ShapeVisual visual{ nullptr };
        wuc::CompositionEllipseGeometry shape{ nullptr };
        wuc::CompositionColorBrush brush{ nullptr };
        std::optional<Vec2> at;
    };
    void place(Dot& dot, std::optional<Vec2> at);
    Color colour() const; // badgeColor, or liveColor for accent dots

    wuc::Compositor compositor_{ nullptr };
    wuc::ContainerVisual parent_{ nullptr };
    wuc::Visual below_{ nullptr };
    std::vector<Dot> dots_;
    bool accent_ = false;
};

} // namespace cup::platform
