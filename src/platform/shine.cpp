#include "platform/shine.h"

#include "platform/element_parts.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cup::platform {
namespace {

// Corner to corner: where each stop is, and its share of the shine. Bright
// at the top left, gone by 30% of the way, back for the last 30%, fainter.
constexpr std::array<float, 4> kAt{ 0.0f, 0.3f, 0.7f, 1.0f };
constexpr std::array<float, 4> kShares{ 1.0f, 0.0f, 0.0f, 0.6f };
constexpr float kWidth = 1.0f; // a hairline: one pixel, not scaled with the display

Color white(float opacity) { return { 255, 255, 255, static_cast<std::uint8_t>(std::lround(255 * std::clamp(opacity, 0.0f, 1.0f))) }; }

} // namespace

Shine makeShine(wuc::Compositor const& c)
{
    Shine shine;
    shine.width = kWidth;
    shine.outline = c.CreateRoundedRectangleGeometry();
    auto brush = c.CreateLinearGradientBrush(); // corner to corner of the outline's bounds
    brush.StartPoint({ 0.0f, 0.0f });
    brush.EndPoint({ 1.0f, 1.0f });
    for (std::size_t i = 0; i < shine.stops.size(); ++i) {
        shine.stops[i] = c.CreateColorGradientStop(kAt[i], toUiColor(white(0)));
        brush.ColorStops().Append(shine.stops[i]);
    }
    shine.shape = c.CreateSpriteShape(shine.outline);
    shine.shape.StrokeBrush(brush);
    shine.shape.StrokeThickness(kWidth);
    return shine;
}

void placeShine(const Shine& shine, Rect area, float cornerRadius)
{
    if (!shine.shape) return;
    // A stroke is centred on its outline: half its width in, it's all inside the edge.
    float half = shine.width / 2;
    float r = std::max(cornerRadius - half, 0.0f);
    shine.outline.Offset({ area.x + half, area.y + half });
    shine.outline.Size({ std::max(area.width - shine.width, 0.0f), std::max(area.height - shine.width, 0.0f) });
    shine.outline.CornerRadius({ r, r });
}

void fadeShine(const Shine& shine, float opacity, Ms duration)
{
    if (!shine.shape) return;
    for (std::size_t i = 0; i < shine.stops.size(); ++i) fadeColor(shine.stops[i], white(opacity * kShares[i]), duration);
}

} // namespace cup::platform
