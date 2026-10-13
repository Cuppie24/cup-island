#include "core/text_wrap.h"

#include "core/slot_layout.h"

#include <algorithm>

namespace cup {

std::string wrapText(const std::string& text, TextStyle style, float width, const MeasureText& measure)
{
    auto widthOf = [&](const std::string& s) {
        Text t{ s, style };
        return measure ? measure(t) : estimateTextWidth(t);
    };
    std::string out = text;
    float space = widthOf(" "), line = 0;
    bool lineEmpty = true;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t wordStart = text.find_first_not_of(' ', at);
        if (wordStart == std::string::npos) break; // trailing spaces: they hang off the last line
        std::size_t wordEnd = std::min(text.find(' ', wordStart), text.size());
        float word = widthOf(text.substr(wordStart, wordEnd - wordStart));
        float spaces = static_cast<float>(wordStart - at) * space;
        if (!lineEmpty && line + spaces + word > width) {
            out[wordStart - 1] = '\n'; // the space right before the word: the rest of the gap hangs at the line's end
            line = word;
        } else {
            line += (lineEmpty ? 0 : spaces) + word;
        }
        lineEmpty = false;
        at = wordEnd;
    }
    return out;
}

int lineCount(const std::string& wrapped) { return 1 + static_cast<int>(std::count(wrapped.begin(), wrapped.end(), '\n')); }

} // namespace cup
