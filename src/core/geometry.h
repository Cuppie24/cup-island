#pragma once
// Minimal 2D math for core/. Platform code converts to/from float2 at the edge.

#include <cmath>

namespace cup {

struct Vec2 {
    float x = 0;
    float y = 0;
    constexpr bool operator==(const Vec2&) const = default;
};

struct Rect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    constexpr bool contains(Vec2 p) const { return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height; }
    constexpr bool operator==(const Rect&) const = default;
};

// On whole pixels, each edge to the nearest: what's drawn there (a button,
// the row behind it) keeps its centre, and things snapped alike line up (decision 071).
// An edge to its whole pixel. First to the nearest 1/64 px: two edges meant
// to meet but computed two ways (one row's top plus its height, the next
// row's top) differ by float noise, 243.49998 against 243.5 at
// islandScale's fractions, and rounded straight to different pixels: two
// pieces of a group's background overlapped by one, a line across it.
inline float snapEdge(float v) { return std::round(std::round(v * 64.0f) / 64.0f); }

inline Rect snapToPixels(Rect r)
{
    float left = snapEdge(r.x), top = snapEdge(r.y);
    return { left, top, snapEdge(r.x + r.width) - left, snapEdge(r.y + r.height) - top };
}

constexpr Vec2 lerp(Vec2 a, Vec2 b, float t)
{
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
}

} // namespace cup
