#include "core/slot_layout.h"

#include "core/item_metrics.h"
#include "core/lists.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

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
    // A Text being edited in place is a focused field with the draft (decision 029).
    if (auto* text = std::get_if<Text>(&item); text && !text->editAction.empty()) {
        const Draft* d = source.draft;
        if (d && d->activityId == source.activityId && d->actionId == text->editAction)
            return TextInput{ .actionId = text->editAction, .value = d->text, .focused = true };
    }
    return item;
}

// The part of `r` inside `clip` (nothing left: zero size at its corner).
Rect clipped(Rect r, Rect clip)
{
    float left = std::max(r.x, clip.x), top = std::max(r.y, clip.y);
    float right = std::min(r.x + r.width, clip.x + clip.width), bottom = std::min(r.y + r.height, clip.y + clip.height);
    return { left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top) };
}

// Rounded content (art, button hover shapes, text fields) only.
float radiusFor(const Item& item, Rect frame, float nestedRadius)
{
    if (!std::holds_alternative<Image>(item) && !std::holds_alternative<Button>(item) && !std::holds_alternative<TextInput>(item)) return 0;
    return std::min(nestedRadius, std::min(frame.width, frame.height) / 2);
}


// A list item's extra height: room above and below its content (Row::listItem).
float listPadding(const Row& row) { return row.listItem ? 2 * tokens().listItemPadding : 0; }

// Each row's ring side (a ring matches the other items in its row; alone, it
// fills the slot's square) and natural height.
struct RowSizes {
    std::vector<float> ringSides, heights;
};

RowSizes rowSizes(const Slot& rows, Rect frame)
{
    RowSizes sizes;
    for (const Row& r : rows) {
        float others = 0;
        for (const auto* items : { &r.items, &r.leading, &r.trailing })
            for (const Item& item : *items)
                if (!isRing(item)) others = std::max(others, heightOf(item, frame, 0));
        float ringSide = others > 0 ? others : std::min(frame.width, frame.height);
        float h = 0;
        for (const auto* items : { &r.items, &r.leading, &r.trailing })
            for (const Item& item : *items) h = std::max(h, heightOf(item, frame, ringSide));
        sizes.ringSides.push_back(ringSide);
        sizes.heights.push_back(h + listPadding(r));
    }
    return sizes;
}

} // namespace

float slotHeight(const Slot& slot, Rect frame) { return planRows(slot, rowSizes(slot, frame).heights, {}).height; }

void placeSlot(const SlotFrame& slot, const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out,
               const MeasureText& measure)
{
    const Tokens& t = tokens();
    const Slot& rows = *slot.slot;

    RowSizes sizes = rowSizes(rows, slot.frame);
    // Lists windowed, scrolled, a row dragged (decision 029); the rest stacked.
    RowPlan plan = planRows(rows, sizes.heights, { source.activityId, source.scrolls, source.listDrag });
    float contentTop = slot.frame.y + (slot.frame.height - plan.height) / 2; // centred vertically

    // Places every row into `target` and returns each row's content span.
    // `spans`: the first pass's spans, for rows that match a neighbour's width.
    auto placeRows = [&](std::vector<Placed>& target, const std::vector<Rect>* spans) {
        std::vector<Rect> contentSpans;
        for (size_t r = 0; r < rows.size(); ++r) {
            const Row& row = rows[r];
            float y = contentTop + plan.rows[r].y;
            std::optional<InList> list = plan.rows[r].list;
            if (list) list->viewport = { slot.frame.x, contentTop + list->viewport.y, slot.frame.width, list->viewport.height };
            // Where this row lays out: the slot, or a neighbour's content span.
            Rect area = slot.frame;
            std::size_t neighbour = row.span == RowSpan::MatchAbove ? r - 1 : r + 1;
            if (spans && row.span != RowSpan::Slot && neighbour < rows.size() && (*spans)[neighbour].width > 0)
                area = { (*spans)[neighbour].x, slot.frame.y, (*spans)[neighbour].width, slot.frame.height };
            Rect background = area; // a list item's hover background spans the row; its content sits inside
            if (row.listItem) area = { area.x + t.listItemInset, area.y, std::max(0.0f, area.width - 2 * t.listItemInset), area.height };
            float rowHeight = sizes.heights[r], ringSide = sizes.ringSides[r];
            float gap = row.loose ? t.looseItemSpacing : t.itemSpacing;

            // Keys number `items`, then `trailing`, then `leading`, so they stay stable as rows grow.
            // A row with an id is named by it, so its visuals follow it when a list reorders.
            std::string rowName = row.id.empty() ? std::to_string(r) : row.id;
            size_t first = target.size();
            auto place = [&](const Item& item, size_t index, float x, float w) {
                float h = heightOf(item, slot.frame, ringSide);
                Placed placed;
                placed.key = slot.name + "/" + rowName + "/" + std::to_string(index);
                placed.item = resolve(item, now, source);
                placed.frame = { x, y + (rowHeight - h) / 2, w, h };
                placed.cornerRadius = radiusFor(item, placed.frame, nestedRadius);
                placed.accent = source.content ? source.content->accent : std::nullopt;
                placed.list = list;
                if (row.listItem) {
                    placed.listRow = Rect{ background.x, y, background.width, rowHeight };
                    placed.listRowRadius = std::min(nestedRadius, rowHeight / 2);
                }
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
            bool editing = std::any_of(target.begin() + static_cast<std::ptrdiff_t>(first), target.end(),
                                       [](const Placed& p) { return std::holds_alternative<TextInput>(p.item); });
            if (row.clickAnywhere && middle > 0 && !editing) { // while its text is edited, clicks go to the field
                float left = target[first].hitFrame.x, right = left;
                for (std::size_t i = first; i < first + middle; ++i) {
                    left = std::min(left, target[i].hitFrame.x);
                    right = std::max(right, target[i].hitFrame.x + target[i].hitFrame.width);
                }
                if (row.listItem) left = std::min(left, background.x); // from the background's edge
                Rect whole{ left, y - t.rowSpacing / 2, right - left, rowHeight + t.rowSpacing };
                for (std::size_t i = first; i < first + middle; ++i)
                    if (std::holds_alternative<Button>(target[i].item)) {
                        target[i].hitFrame = whole;
                        target[i].litByRow = row.listItem; // the lit row says it's clickable; a box inside it would double it
                    }
            }
            // In a list: only what's inside its window can be pointed at.
            if (list)
                for (std::size_t i = first; i < target.size(); ++i) {
                    target[i].hitFrame = clipped(target[i].hitFrame, list->viewport);
                    if (target[i].listRow) target[i].listRow = clipped(*target[i].listRow, list->viewport);
                }
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
