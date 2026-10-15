#include "core/tokens.h"

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

} // namespace cup
