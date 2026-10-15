#include "platform/badge_dots.h"

#include "core/tokens.h"
#include "platform/element_parts.h"

namespace cup::platform {

void BadgeDots::build(wuc::Compositor const& compositor, wuc::ContainerVisual const& parent, wuc::Visual const& below)
{
    compositor_ = compositor;
    parent_ = parent;
    below_ = below;
}

void BadgeDots::show(const std::vector<Vec2>& centres)
{
    if (!compositor_) return;
    if (dots_.size() < centres.size()) dots_.resize(centres.size());
    for (std::size_t i = 0; i < dots_.size(); ++i) place(dots_[i], i < centres.size() ? std::optional<Vec2>(centres[i]) : std::nullopt);
}

void BadgeDots::recolor()
{
    for (Dot& dot : dots_)
        if (dot.brush) fadeColor(dot.brush, tokens().badgeColor);
}

void BadgeDots::place(Dot& dot, std::optional<Vec2> at)
{
    const Tokens& t = tokens();
    if (!dot.visual) {
        dot.shape = compositor_.CreateEllipseGeometry();
        auto fill = compositor_.CreateSpriteShape(dot.shape);
        dot.brush = compositor_.CreateColorBrush(toUiColor(t.badgeColor)); // recolor() fades it
        fill.FillBrush(dot.brush);
        dot.visual = compositor_.CreateShapeVisual();
        dot.visual.Shapes().Append(fill);
        dot.visual.Opacity(0.0f);
        parent_.Children().InsertAbove(dot.visual, below_);
    }
    float r = t.badgeSize / 2;
    dot.shape.Center({ r, r });
    dot.shape.Radius({ r, r });
    dot.visual.Size({ t.badgeSize, t.badgeSize });
    if (at == dot.at) return;
    dot.at = at;
    if (!at) {
        fadeTo(compositor_, dot.visual, 0.0f, t.hoverFadeDuration);
        return;
    }
    // Moves at once and only fades, like a hover mark: no glide to hand off (decision 008).
    dot.visual.Offset({ at->x - r, at->y - r, 0 });
    fadeTo(compositor_, dot.visual, 1.0f, t.hoverFadeDuration);
}

} // namespace cup::platform
