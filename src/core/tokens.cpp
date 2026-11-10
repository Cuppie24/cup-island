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

Color islandFill(const Tokens& t, Color color)
{
    if (!t.glass) return color;
    float share = std::clamp(t.glassOpacity, 0.0f, 1.0f);
    color.a = static_cast<std::uint8_t>(std::lround(color.a * share));
    return color;
}

} // namespace cup
