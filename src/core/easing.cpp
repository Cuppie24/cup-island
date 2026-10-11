#include "core/easing.h"

namespace cup {

float cubicBezierEase(Bezier curve, float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    auto bezier = [](float p1, float p2, float u) {
        float v = 1.0f - u;
        return 3.0f * v * v * u * p1 + 3.0f * v * u * u * p2 + u * u * u;
    };

    // Find the curve parameter whose x equals t by bisection (x is monotonic
    // while both control x's lie in [0, 1]), then take y there.
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < 24; ++i) {
        float mid = (lo + hi) / 2.0f;
        (bezier(curve.c1.x, curve.c2.x, mid) < t ? lo : hi) = mid;
    }
    return bezier(curve.c1.y, curve.c2.y, (lo + hi) / 2.0f);
}

} // namespace cup
