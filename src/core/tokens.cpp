#include "core/tokens.h"

namespace cup {
namespace {
Tokens g_tokens;
}

const Tokens& tokens() { return g_tokens; }
void setTokens(const Tokens& tokens) { g_tokens = tokens; }

} // namespace cup
