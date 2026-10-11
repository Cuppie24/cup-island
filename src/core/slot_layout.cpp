#include "core/slot_layout.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

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
    if (auto* button = std::get_if<Button>(&item)) return button->compact ? t.smallButtonSize : t.buttonSize;
    if (std::holds_alternative<TextInput>(item)) return t.inputHeight;
    // Seekable bars get a taller row to be easy to click; a bar nobody clicks is just its line.
    if (auto* bar = std::get_if<Progress>(&item)) return bar->seekable ? t.progressHitHeight : t.progressHeight;
    if (auto* label = std::get_if<TimeLabel>(&item)) return lineHeight(label->style);
    return t.iconSize; // Icon, Waveform
}

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

Item resolve(const Item& item, Ms now, const Source& source)
{
    if (auto* input = std::get_if<TextInput>(&item)) {
        TextInput shown = *input;
        const Draft* d = source.draft;
        shown.focused = d && d->activityId == source.activityId && d->actionId == input->actionId;
        shown.value = shown.focused ? d->text : "";
        return shown;
    }
    if (auto* label = std::get_if<TimeLabel>(&item)) {
        Ms position = label->timeline.positionAt(now);
        std::string left = formatDuration(label->timeline.duration - position);
        std::string value = label->countdown ? left : label->remaining ? "-" + left : formatDuration(position);
        return Text{ .value = value, .style = label->style, .alignEnd = label->remaining, .marquee = false, .centred = label->countdown };
    }
    return item;
}

// Rounded content (art, button hover shapes, text fields) only.
float radiusFor(const Item& item, Rect frame, float nestedRadius)
{
    if (!std::holds_alternative<Image>(item) && !std::holds_alternative<Button>(item) && !std::holds_alternative<TextInput>(item)) return 0;
    return std::min(nestedRadius, std::min(frame.width, frame.height) / 2);
}


} // namespace

float fontSize(TextStyle style)
{
    const Tokens& t = tokens();
    switch (style) {
    case TextStyle::Title: return t.titleFontSize;
    case TextStyle::Body: return t.bodyFontSize;
    case TextStyle::Caption:
    case TextStyle::Digits: return t.captionFontSize;
    case TextStyle::Display: return t.displayFontSize;
    }
    return t.bodyFontSize;
}

float slotHeight(const Slot& slot, Rect frame)
{
    float total = 0;
    for (const Row& r : slot) {
        float others = 0, rings = 0;
        for (const auto* items : { &r.items, &r.leading, &r.trailing })
            for (const Item& item : *items) {
                if (isRing(item)) rings = std::max(rings, std::min(frame.width, frame.height));
                else others = std::max(others, heightOf(item, frame, 0));
            }
        total += others > 0 ? others : rings;
    }
    return total + tokens().rowSpacing * static_cast<float>(slot.empty() ? 0 : slot.size() - 1);
}

float estimateTextWidth(const Text& text) { return std::ceil(static_cast<float>(text.value.size()) * fontSize(text.style) * 0.5f); }

void placeSlot(const SlotFrame& slot, const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out,
               const MeasureText& measure)
{
    const Tokens& t = tokens();
    const Slot& rows = *slot.slot;

    // A ring matches the other items in its row; alone, it fills the slot's square.
    std::vector<float> ringSides;
    for (const Row& r : rows) {
        float others = 0;
        for (const auto* items : { &r.items, &r.leading, &r.trailing })
            for (const Item& item : *items)
                if (!isRing(item)) others = std::max(others, heightOf(item, slot.frame, 0));
        ringSides.push_back(others > 0 ? others : std::min(slot.frame.width, slot.frame.height));
    }

    std::vector<float> rowHeights;
    float total = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        float h = 0;
        for (const auto* items : { &rows[i].items, &rows[i].leading, &rows[i].trailing })
            for (const Item& item : *items) h = std::max(h, heightOf(item, slot.frame, ringSides[i]));
        rowHeights.push_back(h);
        total += h;
    }
    total += t.rowSpacing * static_cast<float>(rows.empty() ? 0 : rows.size() - 1);

    // Places every row into `target` and returns each row's content span.
    // `spans`: the first pass's spans, for rows that match a neighbour's width.
    auto placeRows = [&](std::vector<Placed>& target, const std::vector<Rect>* spans) {
        std::vector<Rect> contentSpans;
        float y = slot.frame.y + (slot.frame.height - total) / 2; // centred vertically
        for (size_t r = 0; r < rows.size(); ++r) {
            const Row& row = rows[r];
            // Where this row lays out: the slot, or a neighbour's content span.
            Rect area = slot.frame;
            std::size_t neighbour = row.span == RowSpan::MatchAbove ? r - 1 : r + 1;
            if (spans && row.span != RowSpan::Slot && neighbour < rows.size() && (*spans)[neighbour].width > 0)
                area = { (*spans)[neighbour].x, slot.frame.y, (*spans)[neighbour].width, slot.frame.height };
            float rowHeight = rowHeights[r], ringSide = ringSides[r];
            float gap = row.loose ? t.looseItemSpacing : t.itemSpacing;

            // Keys number `items`, then `trailing`, then `leading`, so they stay stable as rows grow.
            size_t first = target.size();
            auto place = [&](const Item& item, size_t index, float x, float w) {
                float h = heightOf(item, slot.frame, ringSide);
                Placed placed;
                placed.key = slot.name + "/" + std::to_string(r) + "/" + std::to_string(index);
                placed.item = resolve(item, now, source);
                placed.frame = { x, y + (rowHeight - h) / 2, w, h };
                placed.cornerRadius = radiusFor(item, placed.frame, nestedRadius);
                placed.accent = source.content ? source.content->accent : std::nullopt;
                placed.scrollAction = row.scrollAction;
                placed.providerId = source.providerId;
                placed.activityId = source.activityId;
                if (auto* label = std::get_if<TimeLabel>(&item)) placed.ticking = label->timeline;
                target.push_back(std::move(placed));
            };
            auto widthOf = [&](const Item& item) { return fixedWidthOf(item, heightOf(item, slot.frame, ringSide), measure); };

            // Edge groups: packed with the normal spacing, kept `gap` away from the middle.
            auto edgeWidth = [&](const std::vector<Item>& group) {
                float w = 0;
                for (const Item& item : group) w += widthOf(item).value_or(0) + t.itemSpacing;
                return group.empty() ? 0.0f : w - t.itemSpacing + gap;
            };
            float leadingWidth = edgeWidth(row.leading), trailingWidth = edgeWidth(row.trailing);
            float available = area.width - leadingWidth - trailingWidth;

            float fixed = 0;
            int stretchCount = 0;
            for (const Item& item : row.items) {
                if (auto w = widthOf(item)) fixed += *w;
                else ++stretchCount;
            }
            float spacing = gap * static_cast<float>(row.items.empty() ? 0 : row.items.size() - 1);
            float used = fixed + spacing;
            float spare = std::max(0.0f, available - used);
            float stretchWidth = stretchCount > 0 ? spare / static_cast<float>(stretchCount) : 0;

            Align align = slot.square ? Align::Center : row.align;
            float x = area.x + leadingWidth;
            if (stretchCount == 0 && align == Align::End) x += spare;
            if (stretchCount == 0 && align == Align::Center) {
                // Centred on the whole row, so edge groups don't pull the middle off
                // centre; nudged sideways only as far as needed to clear them.
                x = area.x + std::clamp((area.width - used) / 2, leadingWidth, leadingWidth + spare);
            }
            for (size_t i = 0; i < row.items.size(); ++i) {
                float w = widthOf(row.items[i]).value_or(stretchWidth);
                place(row.items[i], i, x, w);
                x += w + gap;
            }
            auto placeEdge = [&](const std::vector<Item>& group, size_t firstIndex, float x) {
                for (size_t i = 0; i < group.size(); ++i) {
                    float w = widthOf(group[i]).value_or(0);
                    place(group[i], firstIndex + i, x, w);
                    x += w + t.itemSpacing;
                }
            };
            placeEdge(row.trailing, row.items.size(), area.x + area.width - trailingWidth + gap);
            placeEdge(row.leading, row.items.size() + row.trailing.size(), area.x);
            Rect content{};
            if (target.size() > first) {
                float left = target[first].frame.x, right = left;
                for (size_t i = first; i < target.size(); ++i) {
                    left = std::min(left, target[i].frame.x);
                    right = std::max(right, target[i].frame.x + target[i].frame.width);
                }
                content = { left, y, right - left, rowHeight };
            }
            contentSpans.push_back(content);

            // Click areas meet halfway between neighbours (no dead strips) and reach
            // half a gap past the row's ends and half the row spacing above and below.
            std::vector<Placed*> placed;
            for (size_t i = first; i < target.size(); ++i) placed.push_back(&target[i]);
            std::sort(placed.begin(), placed.end(), [](const Placed* a, const Placed* b) { return a->frame.x < b->frame.x; });
            for (size_t i = 0; i < placed.size(); ++i) {
                const Rect& f = placed[i]->frame;
                float left = i == 0 ? f.x - gap / 2 : (placed[i - 1]->frame.x + placed[i - 1]->frame.width + f.x) / 2;
                float right = i + 1 == placed.size() ? f.x + f.width + gap / 2 : (f.x + f.width + placed[i + 1]->frame.x) / 2;
                placed[i]->hitFrame = { left, y - t.rowSpacing / 2, right - left, rowHeight + t.rowSpacing };
            }
            // A list item: its button takes clicks anywhere across the row's
            // `items` (Row::clickAnywhere). Edge items (a delete button) keep their own.
            std::size_t middle = row.items.size();
            if (row.clickAnywhere && middle > 0) {
                float left = target[first].hitFrame.x, right = left;
                for (std::size_t i = first; i < first + middle; ++i) {
                    left = std::min(left, target[i].hitFrame.x);
                    right = std::max(right, target[i].hitFrame.x + target[i].hitFrame.width);
                }
                Rect whole{ left, y - t.rowSpacing / 2, right - left, rowHeight + t.rowSpacing };
                for (std::size_t i = first; i < first + middle; ++i)
                    if (std::holds_alternative<Button>(target[i].item)) target[i].hitFrame = whole;
            }
            y += rowHeight + t.rowSpacing;
        }
        return contentSpans;
    };

    // Two passes: natural spans first, then the real layout, where a matching
    // row takes its neighbour's span (above or below: either may come first).
    std::vector<Placed> scratch;
    std::vector<Rect> spans = placeRows(scratch, nullptr);
    placeRows(out, &spans);
}

} // namespace cup
