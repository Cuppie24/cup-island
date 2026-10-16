#include "core/slot_layout.h"

#include "core/item_metrics.h"
#include "core/card_overlays.h"
#include "core/grid_layout.h"
#include "core/lists.h"
#include "core/text_wrap.h"
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
        // Not focused: the text it was left with, if any (decision 050).
        if (shown.focused) shown.value = d->text;
        else if (source.keptText)
            if (auto kept = source.keptText->find(keptKey(source.activityId, input->actionId)); kept != source.keptText->end())
                shown.value = kept->second;
        if (shown.focused) shown.caret = d->caret, shown.anchor = d->anchor;
        return shown;
    }
    if (auto* label = std::get_if<TimeLabel>(&item)) {
        Ms position = label->timeline.positionAt(now);
        std::string left = formatDuration(label->timeline.duration - position);
        std::string value = label->countdown ? left : label->remaining ? "-" + left : formatDuration(position);
        return Text{ .value = value, .style = label->style, .alignEnd = label->remaining, .marquee = false, .centred = label->countdown };
    }
    // A Text being edited in place is a focused field with the draft (decisions 029, 043).
    if (auto* text = std::get_if<Text>(&item); text && !text->editAction.empty()) {
        const Draft* d = source.draft;
        if (d && d->activityId == source.activityId && d->actionId == text->editAction)
            return TextInput{ .actionId = text->editAction, .value = d->text, .focused = true, .caret = d->caret, .anchor = d->anchor,
                              .inPlace = true };
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
    auto* text = std::get_if<Text>(&item);
    bool hinted = text && text->editHint; // its hover shape is rounded like a field's (decision 063)
    if (!std::holds_alternative<Image>(item) && !std::holds_alternative<Button>(item) && !std::holds_alternative<TextInput>(item) && !hinted) return 0;
    return std::min(nestedRadius, std::min(frame.width, frame.height) / 2);
}


// A list item's extra height: room above and below its content (Row::listItem).
float listPadding(const Row& row) { return row.listItem ? 2 * tokens().listItemPadding : 0; }

// Room above a group's first row (Row::section, decision 037).
float sectionGap(const Row& row) { return row.section ? tokens().sectionGap : 0; }

// Each row's ring side (a ring matches the other items in its row; alone, it
// fills the slot's square) and natural height.
struct RowSizes {
    std::vector<float> ringSides, heights;
};

RowSizes rowSizes(const Slot& rows, Rect frame)
{
    RowSizes sizes;
    for (const Row& r : rows) {
        if (!r.cells.empty()) { // a grid's line (decision 063)
            sizes.ringSides.push_back(0);
            sizes.heights.push_back(tokens().gridCellHeight);
            continue;
        }
        float others = 0;
        for (const auto* items : { &r.items, &r.leading, &r.trailing })
            for (const Item& item : *items)
                if (!isRing(item)) others = std::max(others, heightOf(item, frame, 0));
        float ringSide = others > 0 ? others : std::min(frame.width, frame.height);
        float h = 0;
        for (const auto* items : { &r.items, &r.leading, &r.trailing })
            for (const Item& item : *items) h = std::max(h, heightOf(item, frame, ringSide));
        sizes.ringSides.push_back(ringSide);
        sizes.heights.push_back(h + listPadding(r) + sectionGap(r));
    }
    return sizes;
}

// Places the slot's rows into `out`; returns the height they take.
float layRows(const SlotFrame& slot, const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out,
                const MeasureText& measure)
{
    const Tokens& t = tokens();
    // Grids wrapped into lines of cells (decision 063); a reveal names a cell, so it scrolls to the cell's line.
    WrappedGrids grids = wrapGrids(*slot.slot, slot.frame.width);
    const Slot& rows = grids.any ? grids.rows : *slot.slot;
    std::map<std::string, ListScroll> gridScrolls;
    const std::map<std::string, ListScroll>* scrolls = source.scrolls;
    if (grids.any && scrolls) {
        gridScrolls = *scrolls;
        for (const auto& [list, columns] : grids.columns)
            if (auto it = gridScrolls.find(listKey(source.activityId, list)); it != gridScrolls.end() && it->second.reveal)
                it->second.reveal = *it->second.reveal / columns;
        scrolls = &gridScrolls;
    }

    RowSizes sizes = rowSizes(rows, slot.frame);
    const std::vector<float> oneLine = sizes.heights; // a list's window counts rows of one line (decision 045)
    // Lists windowed, scrolled, a row dragged (decision 029); the rest stacked.
    ListState lists{ source.activityId, scrolls, source.listDrag };
    float fill = slot.fill ? slot.frame.height : 0;
    RowPlan plan = planRows(rows, sizes.heights, lists, &oneLine, fill);
    // Centred vertically; a filled slot (a Large card) starts at its top.
    auto topOf = [&](const RowPlan& p) { return slot.fill ? slot.frame.y : slot.frame.y + (slot.frame.height - p.height) / 2; };
    float contentTop = topOf(plan);
    std::vector<float> wrapped(rows.size(), 0); // each row's wrapping text's height, once its width is known

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
            if (row.listItem || row.inset) {
                // What's at an edge sits listItemTextInset in, clear of the background's
                // rounded end (decision 030), and in line with the headings and names of
                // the rows around it: text, a theme's swatch. A button there (a to-do's
                // checkbox) keeps listItemInset: its own round shape nests in the end.
                auto inset = [&](const std::vector<Item>& edge, const Item* outer) {
                    bool control = outer && (std::holds_alternative<Button>(*outer) || std::holds_alternative<TextInput>(*outer) ||
                                             std::holds_alternative<Switch>(*outer));
                    return edge.empty() && outer && !control ? t.listItemTextInset : t.listItemInset;
                };
                float left = inset(row.leading, row.items.empty() ? nullptr : &row.items.front());
                float right = inset(row.trailing, row.items.empty() ? nullptr : &row.items.back());
                area = { area.x + left, area.y, std::max(0.0f, area.width - left - right), area.height };
            }
            float rowHeight = sizes.heights[r], ringSide = sizes.ringSides[r];
            float gap = row.loose ? t.looseItemSpacing : t.itemSpacing;

            // Keys number `items`, then `trailing`, then `leading`, so they stay stable as rows grow.
            // A row with an id is named by it, so its visuals follow it when a list reorders.
            std::string rowName = row.id.empty() ? std::to_string(r) : row.id;
            if (!row.cells.empty()) { // a grid's line: its cells side by side (decision 063)
                placeCells(row, grids.firstCell[r], { area.x, y, area.width, rowHeight }, list, slot.name + "/" + rowName, source,
                           nestedRadius, now, target, measure);
                contentSpans.push_back({ area.x, y, area.width, rowHeight });
                continue;
            }
            size_t first = target.size();
            auto place = [&](const Item& item, size_t index, float x, float w) {
                float h = heightOf(item, slot.frame, ringSide);
                Placed placed;
                placed.key = slot.name + "/" + rowName + "/" + std::to_string(index);
                placed.item = resolve(item, now, source);
                // Wrapping text (decision 045): its lines at the width it got (less its edit inset),
                // and as tall as they are. The row grows to fit in the next pass.
                if (auto* text = std::get_if<Text>(&item); text && text->wrap) {
                    float room = std::max(0.0f, w - (text->editAction.empty() ? 0 : 2 * t.editInset));
                    if (auto* shown = std::get_if<Text>(&placed.item)) {
                        shown->wrapped = wrapText(shown->value, shown->style, room, measure);
                        h = static_cast<float>(lineCount(shown->wrapped)) * lineHeight(shown->style);
                    } else if (auto* field = std::get_if<TextInput>(&placed.item)) {
                        field->wrapped = wrapText(field->value, TextStyle::Body, room, measure); // the field's font
                        h = static_cast<float>(lineCount(field->wrapped)) * lineHeight(TextStyle::Body);
                    }
                    wrapped[r] = std::max(wrapped[r], h);
                }
                float above = sectionGap(row); // the gap is empty: content centres in the rest
                placed.frame = { x, y + above + (rowHeight - above - h) / 2, w, h };
                // Editable text owns its whole stretch, but is drawn editInset in from each side.
                // Edited in place, the field fills that stretch (so its background keeps the row's
                // gaps to the checkbox and delete button) and is as tall as the row's content, with
                // the text where it was, centred in it (decision 043).
                if (auto* text = std::get_if<Text>(&item); text && !text->editAction.empty()) {
                    if (auto* field = std::get_if<TextInput>(&placed.item); field && field->inPlace) {
                        float tall = std::max(h, rowHeight - above - listPadding(row));
                        placed.frame = { x, y + above + (rowHeight - above - tall) / 2, w, tall };
                    } else {
                        placed.frame.x += t.editInset;
                        placed.frame.width = std::max(0.0f, w - 2 * t.editInset);
                    }
                }
                // In a list item, rounded content nests in the row's background, not the
                // card's corner: its radius is the row's less the inset (decision 006).
                float rowRadius = std::min(nestedRadius, rowHeight / 2);
                placed.cornerRadius = radiusFor(placed.item, placed.frame, row.listItem ? std::max(0.0f, rowRadius - t.listItemInset) : nestedRadius);
                placed.accent = source.content ? source.content->accent : std::nullopt;
                placed.list = list;
                if (row.listItem) {
                    placed.listRow = Rect{ background.x, y, background.width, rowHeight };
                    placed.listRowRadius = rowRadius;
                }
                placed.providerId = source.providerId;
                placed.activityId = source.activityId;
                placed.openAction = row.openAction;
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
            // Only texts stretch (a setting's name and its value): each gets its
            // own width and the last the rest; short of room, each gives up its
            // share. An even split would cut "Keyboard shortcuts" beside an
            // empty value (decision 037).
            std::vector<float> textWidths;
            if (stretchCount >= 2 && std::all_of(row.items.begin(), row.items.end(), [&](const Item& item) {
                    return widthOf(item) || std::holds_alternative<Text>(item);
                })) {
                float natural = 0;
                for (const Item& item : row.items)
                    if (auto* text = std::get_if<Text>(&item); text && !widthOf(item)) {
                        textWidths.push_back((measure ? measure(*text) : estimateTextWidth(*text)) + 1); // +1: rounding never trips the "…"
                        natural += textWidths.back();
                    }
                if (natural <= spare) textWidths.back() += spare - natural;
                else for (float& w : textWidths) w *= spare / natural;
            }
            std::size_t nextText = 0;

            Align align = slot.square ? Align::Center : row.align;
            float x = area.x + leadingWidth;
            if (stretchCount == 0 && align == Align::End) x += spare;
            if (stretchCount == 0 && align == Align::Center) {
                // Centred on the whole row, so edge groups don't pull the middle off
                // centre; nudged sideways only as far as needed to clear them.
                x = area.x + std::clamp((area.width - used) / 2, leadingWidth, leadingWidth + spare);
            }
            for (size_t i = 0; i < row.items.size(); ++i) {
                // A text takes the next of the texts' widths; a fixed item (a tag, a button) takes none of them.
                std::optional<float> fixedW = widthOf(row.items[i]);
                float w = fixedW ? *fixedW : textWidths.empty() ? stretchWidth : textWidths[nextText++];
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
            // `items` (Row::clickAnywhere: a row of Settings). Edge items keep their own.
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
                    if (std::holds_alternative<Button>(target[i].item) || std::holds_alternative<Switch>(target[i].item)) {
                        target[i].hitFrame = whole;
                        target[i].litByRow = row.listItem; // the lit row says it's clickable; a box inside it would double it
                    }
            }
            // Otherwise a list item's first and last items reach its edges: a
            // to-do's checkbox takes clicks from the row's very start (decision 043).
            else if (row.listItem && !placed.empty()) {
                Rect& front = placed.front()->hitFrame;
                Rect& back = placed.back()->hitFrame;
                float right = front.x + front.width;
                front.x = std::min(front.x, background.x), front.width = right - front.x;
                back.width = std::max(back.width, background.x + background.width - back.x);
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
    // The first also finds how tall wrapping texts are: their rows grow.
    std::vector<Placed> scratch;
    std::vector<Rect> spans = placeRows(scratch, nullptr);
    bool grew = false;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        float needed = wrapped[r] + listPadding(rows[r]) + sectionGap(rows[r]);
        if (needed > sizes.heights[r]) sizes.heights[r] = needed, grew = true;
    }
    if (grew) {
        plan = planRows(rows, sizes.heights, lists, &oneLine, fill);
        contentTop = topOf(plan);
    }
    placeRows(out, &spans);
    return plan.height;
}

} // namespace

float slotHeight(const Slot& slot, Rect frame, const Source& source, const MeasureText& measure)
{
    std::vector<Placed> scratch;
    return layRows({ "", &slot, frame }, source, 0, Ms{ 0 }, scratch, measure);
}

void placeSlot(const SlotFrame& slot, const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out,
               const MeasureText& measure)
{
    layRows(slot, source, nestedRadius, now, out, measure);
}

} // namespace cup
