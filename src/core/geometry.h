#pragma once
// Minimal 2D math for core/. Platform code converts to/from float2 at the edge.

namespace cup {

struct Vec2 {
    float x = 0;
    float y = 0;
    constexpr bool operator==(const Vec2&) const = default;
};

constexpr Vec2 lerp(Vec2 a, Vec2 b, float t)
{
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
}

} // namespace cup
