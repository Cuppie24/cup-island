#pragma once
// Notification badges on the island (decision 038): a red dot at each of the
// scene's badge centres (core/badges.h), over the content. Dots fade in and
// out; they're made on first use and kept, like the tile marks.

#include "core/geometry.h"
#include "platform/win.h"

#include <optional>
#include <vector>

namespace cup::platform {

class BadgeDots {
public:
    // Dots are added to `parent`, above `below` (the content), so content
    // added later still lies under them.
    void build(wuc::Compositor const& compositor, wuc::ContainerVisual const& parent, wuc::Visual const& below);
    void show(const std::vector<Vec2>& centres); // pill coordinates; the rest fade out

private:
    struct Dot {
        wuc::ShapeVisual visual{ nullptr };
        wuc::CompositionEllipseGeometry shape{ nullptr };
        wuc::CompositionColorBrush brush{ nullptr };
        std::optional<Vec2> at;
    };
    void place(Dot& dot, std::optional<Vec2> at);

    wuc::Compositor compositor_{ nullptr };
    wuc::ContainerVisual parent_{ nullptr };
    wuc::Visual below_{ nullptr };
    std::vector<Dot> dots_;
};

} // namespace cup::platform
