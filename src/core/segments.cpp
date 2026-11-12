#include "core/segments.h"

#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {

Text segmentText(const Segments& segments, std::size_t option)
{
    return Text{ .value = option < segments.options.size() ? segments.options[option] : "", .style = TextStyle::Caption, .centred = true };
}

float segmentsWidth(const Segments& segments, const MeasureText& measure)
{
    const Tokens& t = tokens();
    float widest = 0;
    for (std::size_t i = 0; i < segments.options.size(); ++i) {
        Text text = segmentText(segments, i);
        widest = std::max(widest, std::ceil(measure ? measure(text) : estimateTextWidth(text)) + 1);
    }
    return static_cast<float>(segments.options.size()) * (widest + 2 * t.segmentPadding) + 2 * t.segmentInset;
}

Rect segmentFrame(const Segments& segments, Rect frame, std::size_t option)
{
    float inset = tokens().segmentInset;
    float n = static_cast<float>(std::max<std::size_t>(1, segments.options.size()));
    float share = std::max(0.0f, frame.width - 2 * inset) / n;
    return { frame.x + inset + share * static_cast<float>(option), frame.y + inset, share, std::max(0.0f, frame.height - 2 * inset) };
}

std::optional<std::size_t> segmentAt(const Segments& segments, Rect frame, float x)
{
    if (segments.options.empty()) return std::nullopt;
    Rect first = segmentFrame(segments, frame, 0);
    if (first.width <= 0) return 0;
    auto i = static_cast<long>(std::floor((x - first.x) / first.width));
    return static_cast<std::size_t>(std::clamp<long>(i, 0, static_cast<long>(segments.options.size()) - 1));
}

std::string segmentAction(const Segments& segments, std::size_t option) { return segments.actionId + std::to_string(option); }

} // namespace cup
