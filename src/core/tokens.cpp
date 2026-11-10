#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {
Tokens g_tokens;
}

const Tokens& tokens() { return g_tokens; }
void setTokens(const Tokens& tokens) { g_tokens = tokens; }

Ms recolorDuration(const Tokens& t)
{
    switch (t.fadeSpeed) {
    case FadeSpeed::Fast: return t.recolorFast;
    case FadeSpeed::Slow: return t.recolorSlow;
    case FadeSpeed::Medium: break;
    }
    return t.recolorMedium;
}

Color islandFill(const Tokens& t, Color color, bool evenBlur)
{
    if (!t.glass) return color;
    float share = std::clamp(evenBlur ? t.glassOpacity : t.glassPlainOpacity, 0.0f, 1.0f);
    color.a = static_cast<std::uint8_t>(std::lround(color.a * share));
    return color;
}

float glowFade(const Tokens& t, float up)
{
    return std::pow(1.0f - std::clamp(up, 0.0f, 1.0f), std::max(t.glassGlowCurve, 0.0f));
}

bool pale(Color c)
{
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b > 127.5f;
}

Tokens withGlass(Tokens t, bool picked)
{
    t.glass = picked && !pale(t.islandColor);
    return t;
}

} // namespace cup
