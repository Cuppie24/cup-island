#include "core/keycaps.h"

#include "core/item_metrics.h"
#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>
#include <sstream>

namespace cup {

std::vector<KeyPart> keyParts(const std::string& text)
{
    std::vector<KeyPart> parts;
    std::istringstream words(text);
    std::string word;
    while (words >> word) {
        if (word == "or" || word == "to" || word == "Double" || word == "then") {
            parts.push_back({ word, false });
            continue;
        }
        // "+" joins keys of one chord; a "+" with no key before it is the Plus key ("Ctrl++").
        std::string key;
        for (char c : word) {
            if (c != '+' || key.empty()) key += c;
            else parts.push_back({ key, true }), key.clear();
        }
        if (!key.empty()) parts.push_back({ key, true });
    }
    return parts;
}

Text keyPartText(const KeyPart& part) { return Text{ .value = part.label, .style = TextStyle::Caption, .centred = part.cap }; }

float keyPartWidth(const KeyPart& part, const MeasureText& measure)
{
    const Tokens& t = tokens();
    Text text = keyPartText(part);
    float w = std::ceil(measure ? measure(text) : estimateTextWidth(text)) + 1; // +1: rounding never trips the "…"
    return part.cap ? std::max(t.keyCapHeight, w + 2 * t.keyCapPadding) : w;
}

float keysWidth(const Keys& keys, const MeasureText& measure)
{
    auto parts = keyParts(keys.text);
    float w = 0;
    for (const KeyPart& part : parts) w += keyPartWidth(part, measure);
    return w + tokens().keyCapGap * static_cast<float>(parts.empty() ? 0 : parts.size() - 1);
}

} // namespace cup
