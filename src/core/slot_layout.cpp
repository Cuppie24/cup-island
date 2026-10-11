#include "core/slot_layout.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

float lineHeight(TextStyle style) { return std::ceil(fontSize(style) * 1.3f); }

// Natural height of an item. Images fill the slot's square.
float heightOf(const Item& item, Rect slot)
{
    const Tokens& t = tokens();
    if (auto* text = std::get_if<Text>(&item)) return lineHeight(text->style);
    if (std::holds_alternative<Image>(item)) return std::min(slot.width, slot.height);
    if (std::holds_alternative<Button>(item)) return t.buttonSize;
    if (std::holds_alternative<Progress>(item)) return t.progressHitHeight;
    if (std::holds_alternative<TimeLabel>(item)) return lineHeight(TextStyle::Digits);
    return t.iconSize; // Icon, Waveform
}

// Fixed width, or nothing for items that stretch (Text, Progress).
std::optional<float> fixedWidthOf(const Item& item, float height)
{
    const Tokens& t = tokens();
    if (std::holds_alternative<Text>(item) || std::holds_alternative<Progress>(item)) return std::nullopt;
    if (std::holds_alternative<Image>(item)) return height;
    if (std::holds_alternative<Button>(item)) return t.buttonSize;
    if (std::holds_alternative<TimeLabel>(item)) return t.timeLabelWidth;
    return t.iconSize; // Icon, Waveform
}

Item resolve(const Item& item, Ms now)
{
    if (auto* label = std::get_if<TimeLabel>(&item)) {
        Ms position = label->timeline.positionAt(now);
        std::string left = formatDuration(label->timeline.duration - position);
        std::string value = label->countdown ? left : label->remaining ? "-" + left : formatDuration(position);
        return Text{ value, TextStyle::Digits, label->remaining || label->countdown };
    }
    return item;
}

// Rounded content (art, button hover circles) only.
float radiusFor(const Item& item, Rect frame, float nestedRadius)
{
    if (!std::holds_alternative<Image>(item) && !std::holds_alternative<Button>(item)) return 0;
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
    }
    return t.bodyFontSize;
}

void placeSlot(const SlotFrame& slot, const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out)
{
    const Tokens& t = tokens();
    const Slot& rows = *slot.slot;

    std::vector<float> rowHeights;
    float total = 0;
    for (const Row& r : rows) {
        float h = 0;
        for (const auto* items : { &r.items, &r.leading, &r.trailing })
            for (const Item& item : *items) h = std::max(h, heightOf(item, slot.frame));
        rowHeights.push_back(h);
        total += h;
    }
    total += t.rowSpacing * static_cast<float>(rows.empty() ? 0 : rows.size() - 1);

    float y = slot.frame.y + (slot.frame.height - total) / 2; // centred vertically
    for (size_t r = 0; r < rows.size(); ++r) {
        const Row& row = rows[r];
        float rowHeight = rowHeights[r];
        float gap = row.loose ? t.looseItemSpacing : t.itemSpacing;

        // Keys number `items`, then `trailing`, then `leading`, so they stay stable as rows grow.
        size_t first = out.size();
        auto place = [&](const Item& item, size_t index, float x, float w) {
            float h = heightOf(item, slot.frame);
            Placed placed;
            placed.key = slot.name + "/" + std::to_string(r) + "/" + std::to_string(index);
            placed.item = resolve(item, now);
            placed.frame = { x, y + (rowHeight - h) / 2, w, h };
            placed.cornerRadius = radiusFor(item, placed.frame, nestedRadius);
            placed.providerId = source.providerId;
            placed.activityId = source.activityId;
            if (auto* label = std::get_if<TimeLabel>(&item)) placed.ticking = label->timeline;
            out.push_back(std::move(placed));
        };
        auto widthOf = [&](const Item& item) { return fixedWidthOf(item, heightOf(item, slot.frame)); };

        // Edge groups: packed with the normal spacing, kept `gap` away from the middle.
        auto edgeWidth = [&](const std::vector<Item>& group) {
            float w = 0;
            for (const Item& item : group) w += widthOf(item).value_or(0) + t.itemSpacing;
            return group.empty() ? 0.0f : w - t.itemSpacing + gap;
        };
        float leadingWidth = edgeWidth(row.leading), trailingWidth = edgeWidth(row.trailing);
        float available = slot.frame.width - leadingWidth - trailingWidth;

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
        float x = slot.frame.x + leadingWidth;
        if (stretchCount == 0 && align == Align::End) x += spare;
        if (stretchCount == 0 && align == Align::Center) {
            // Centred on the whole row, so edge groups don't pull the middle off
            // centre; nudged sideways only as far as needed to clear them.
            x = slot.frame.x + std::clamp((slot.frame.width - used) / 2, leadingWidth, leadingWidth + spare);
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
        placeEdge(row.trailing, row.items.size(), slot.frame.x + slot.frame.width - trailingWidth + gap);
        placeEdge(row.leading, row.items.size() + row.trailing.size(), slot.frame.x);

        // Click areas meet halfway between neighbours (no dead strips) and reach
        // half a gap past the row's ends and half the row spacing above and below.
        std::vector<Placed*> placed;
        for (size_t i = first; i < out.size(); ++i) placed.push_back(&out[i]);
        std::sort(placed.begin(), placed.end(), [](const Placed* a, const Placed* b) { return a->frame.x < b->frame.x; });
        for (size_t i = 0; i < placed.size(); ++i) {
            const Rect& f = placed[i]->frame;
            float left = i == 0 ? f.x - gap / 2 : (placed[i - 1]->frame.x + placed[i - 1]->frame.width + f.x) / 2;
            float right = i + 1 == placed.size() ? f.x + f.width + gap / 2 : (f.x + f.width + placed[i + 1]->frame.x) / 2;
            placed[i]->hitFrame = { left, y - t.rowSpacing / 2, right - left, rowHeight + t.rowSpacing };
        }
        y += rowHeight + t.rowSpacing;
    }
}

} // namespace cup
