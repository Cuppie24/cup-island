#include "core/liquid_glass.h"

#include <algorithm>

namespace cup {

LiquidShape liquidShape(Rect body, bool attached, bool leftEar, bool rightEar, const Tokens& t)
{
    if (body.width <= 0 || body.height <= 0) return {};
    // The same curve as the body's background (pillCornerRadius).
    float radius = std::min(std::min(body.width, body.height) / 2, t.maxCornerRadius);
    float top = body.y;
    // The flare's size, as the body draws it (blob_shape's kEarSize): no taller than the shape.
    float ear = attached ? std::max(0.0f, std::min(t.flareRadius, body.height)) : 0.0f;
    if (attached) {
        // Past the rounded top and the whole rim: inside the body, nothing is
        // within the rim's reach of the top edge any more.
        float past = radius + std::max(t.glassLiquidRim, 0.0f);
        body.y -= past;
        body.height += past;
    }
    return { body, radius, top, leftEar ? ear : 0.0f, rightEar ? ear : 0.0f };
}

bool liquidPicked(const Tokens& t) { return t.glass && t.glassStyle == GlassStyle::Liquid; }

bool liquidDrawn(const Tokens& t, bool cardOpen) { return liquidPicked(t) && cardOpen; }

bool liquidRing(const Button& button) { return button.large && !button.primary; }

Rect morphRect(Rect from, Rect to, const Motion& motion, Ms elapsed)
{
    if (motion.duration <= Ms{ 0 } || elapsed >= motion.duration) return to;
    float t = std::max(0.0f, static_cast<float>(elapsed.count()) / static_cast<float>(motion.duration.count()));
    float p = springProgress(motion, t);
    auto mix = [p](float a, float b) { return a + (b - a) * p; };
    return { mix(from.x, to.x), mix(from.y, to.y), std::max(0.0f, mix(from.width, to.width)), std::max(0.0f, mix(from.height, to.height)) };
}

Rect liquidBounds(const std::vector<LiquidShape>& shapes, float margin)
{
    bool any = false;
    float left = 0, top = 0, right = 0, bottom = 0;
    for (const LiquidShape& shape : shapes) {
        const Rect& f = shape.frame;
        if (f.width <= 0 || f.height <= 0) continue;
        left = any ? std::min(left, f.x) : f.x;
        top = any ? std::min(top, f.y) : f.y;
        right = any ? std::max(right, f.x + f.width) : f.x + f.width;
        bottom = any ? std::max(bottom, f.y + f.height) : f.y + f.height;
        any = true;
    }
    if (!any) return {};
    return { left - margin, top - margin, right - left + 2 * margin, bottom - top + 2 * margin };
}

} // namespace cup
