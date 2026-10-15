#include "core/item_metrics.h"

#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

// The widest the label can get, so its frame never changes while it ticks:
// its shape ("25:00") with every digit replaced by the widest digit in this
// font and style. Windows' UI font has proportional digits ("4" is wider
// than "0"), so "00:00" alone isn't enough: "24:44" got cut to "2…".
std::string widestDigits(std::string s, TextStyle style, const MeasureText& measure)
{
    char widest = '0';
    float widestWidth = -1;
    for (char d = '0'; d <= '9'; ++d) {
        Text digit{ std::string(1, d), style };
        float w = measure ? measure(digit) : estimateTextWidth(digit);
        if (w > widestWidth) widestWidth = w, widest = d;
    }
    for (char& c : s)
        if (c >= '0' && c <= '9') c = widest;
    return s;
}

} // namespace

float lineHeight(TextStyle style) { return std::ceil(fontSize(style) * 1.3f); }

bool isRing(const Item& item)
{
    auto* progress = std::get_if<Progress>(&item);
    return progress && progress->shape == ProgressShape::Ring;
}

// Natural height of an item. Images fill the slot's square; a ring is
// `ringSide` (its row's height, or the slot's square when alone).
float heightOf(const Item& item, Rect slot, float ringSide)
{
    const Tokens& t = tokens();
    if (auto* text = std::get_if<Text>(&item)) return lineHeight(text->style);
    if (std::holds_alternative<Image>(item)) return std::min(slot.width, slot.height);
    if (isRing(item)) return ringSide;
    if (std::holds_alternative<Steps>(item)) return t.stepDotSize;
    if (std::holds_alternative<Swatch>(item)) return t.swatchSize;
    if (std::holds_alternative<Switch>(item)) return t.switchSize.y;
    if (std::holds_alternative<Tag>(item)) return t.tagHeight;
    if (auto* button = std::get_if<Button>(&item)) return button->compact ? t.smallButtonSize : t.buttonSize;
    if (std::holds_alternative<TextInput>(item)) return t.inputHeight;
    // Seekable bars get a taller row to be easy to click; a bar nobody clicks is just its line.
    if (auto* bar = std::get_if<Progress>(&item)) return bar->seekable ? t.progressHitHeight : t.progressHeight;
    if (auto* label = std::get_if<TimeLabel>(&item)) return lineHeight(label->style);
    return t.iconSize; // Icon, Waveform
}

// Fixed width, or nothing for items that stretch (Text, Progress).
std::optional<float> fixedWidthOf(const Item& item, float height, const MeasureText& measure)
{
    const Tokens& t = tokens();
    if (std::holds_alternative<Text>(item) || std::holds_alternative<TextInput>(item) ||
        (std::holds_alternative<Progress>(item) && !isRing(item)))
        return std::nullopt;
    if (std::holds_alternative<Image>(item) || isRing(item)) return height;
    if (auto* steps = std::get_if<Steps>(&item)) {
        float n = static_cast<float>(std::max(steps->count, 0));
        return std::max(0.0f, n * t.stepDotSize + (n - 1) * t.stepDotGap);
    }
    if (std::holds_alternative<Swatch>(item)) return t.swatchSize;
    if (std::holds_alternative<Switch>(item)) return t.switchSize.x;
    if (auto* tag = std::get_if<Tag>(&item)) {
        Text text{ tag->text, TextStyle::Caption };
        return (measure ? measure(text) : estimateTextWidth(text)) + 2 * t.tagPadding;
    }
    if (auto* button = std::get_if<Button>(&item)) {
        if (button->label.empty()) return button->compact ? t.smallButtonSize : t.buttonSize;
        Text text{ button->label, TextStyle::Body };
        return (measure ? measure(text) : estimateTextWidth(text)) + 2 * t.buttonLabelPadding;
    }
    if (auto* label = std::get_if<TimeLabel>(&item)) {
        if (label->style == TextStyle::Digits) return t.timeLabelWidth; // a column, so rows of labels line up
        std::string widest = widestDigits(formatDuration(label->timeline.duration), label->style, measure);
        if (label->remaining && !label->countdown) widest = "-" + widest;
        Text text{ widest, label->style };
        return (measure ? measure(text) : estimateTextWidth(text)) + 1; // +1: rounding never trips the "…"
    }
    return t.iconSize; // Icon, Waveform
}

float fontSize(TextStyle style)
{
    const Tokens& t = tokens();
    switch (style) {
    case TextStyle::Title: return t.titleFontSize;
    case TextStyle::Body: return t.bodyFontSize;
    case TextStyle::Caption:
    case TextStyle::Digits: return t.captionFontSize;
    case TextStyle::Display: return t.displayFontSize;
    case TextStyle::Heading: return t.headingFontSize;
    }
    return t.bodyFontSize;
}

float estimateTextWidth(const Text& text) { return std::ceil(static_cast<float>(text.value.size()) * fontSize(text.style) * 0.5f); }

} // namespace cup
