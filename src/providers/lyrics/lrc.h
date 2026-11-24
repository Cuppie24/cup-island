#pragma once
// LRC, the text format timed lyrics come in (decision 180): a line of the
// song per line of text, each after the time it starts, "[01:02.50] words".
// Pure.

#include "core/content.h"

#include <string_view>
#include <vector>

namespace cup {

// The lines by time. A line with several times ("[00:10.00][01:10.00] la")
// is sung at each; a time with no words is a break and is kept, so the line
// before it doesn't stay up through it. Tags that aren't times ("[ar: …]")
// are skipped, but "[offset: ±ms]" shifts every time (positive: earlier).
// Text with no times at all gives nothing: there's no saying when to show it.
std::vector<LyricLine> parseLrc(std::string_view text);

} // namespace cup
