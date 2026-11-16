#include "core/window_page.h"

#include "core/groups.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace cup {
namespace {

TextStyle windowStyle(TextStyle style)
{
    switch (style) {
    case TextStyle::Title: return TextStyle::WindowTitle;
    case TextStyle::Body: return TextStyle::WindowBody;
    case TextStyle::Caption: return TextStyle::WindowCaption;
    default: return style;
    }
}

// A section's title row: its Heading (edited in place when it says so), what
// goes beside it, and what goes at its end.
Row titleRow(const WindowSection& section)
{
    Row r{ .items = { Text{ .value = section.title, .style = TextStyle::Heading, .editAction = section.titleEdit,
                            .editMaxLength = section.titleMaxLength } },
           .trailing = section.titleTrailing };
    for (const Item& item : section.titleItems) r.items.push_back(item);
    return r;
}

bool hasTitle(const WindowSection& section) { return !section.title.empty() || !section.titleTrailing.empty(); }

// A section's rows as the page shows them: the page scrolls as a whole (window_content.h), not its lists.
std::vector<Row> unlisted(std::vector<Row> rows)
{
    for (Row& r : rows) {
        r.list.clear();
        r.visibleRows = 0;
    }
    return rows;
}

// A card: its title inside it, on the same background as its rows (decision 117).
Slot cardSlot(const WindowSection& section)
{
    Slot slot;
    if (hasTitle(section)) {
        slot.push_back(titleRow(section));
        slot.back().id = "title"; // one card, one slot: its rows' ids stay unique
    }
    for (Row& r : unlisted(section.rows)) slot.push_back(std::move(r));
    for (Row& r : slot) r.grouped = true;
    return windowStyled(std::move(slot));
}

std::string cardName(const WindowSection& section, std::size_t index)
{
    return "card." + (section.id.empty() ? std::to_string(index) : section.id);
}

} // namespace

Slot windowStyled(Slot slot)
{
    for (Row& r : slot)
        for (auto* items : { &r.items, &r.leading, &r.trailing })
            for (Item& item : *items) {
                if (auto* text = std::get_if<Text>(&item)) text->style = windowStyle(text->style);
                else if (auto* label = std::get_if<TimeLabel>(&item)) label->style = windowStyle(label->style);
            }
    return slot;
}

CardGrid cardGrid(float available)
{
    const Tokens& t = tokens();
    available = std::max(0.0f, available);
    int columns = std::max(1, static_cast<int>(std::floor((available + t.windowCardGap) / (t.windowCardWidth + t.windowCardGap))));
    return { columns, std::floor(std::min(available, t.windowCardWidth)) };
}

std::vector<Rect> cardFrames(const CardGrid& grid, const std::vector<float>& heights, Vec2 topLeft)
{
    float gap = tokens().windowCardGap;
    std::vector<Rect> frames;
    float y = topLeft.y;
    for (std::size_t first = 0; first < heights.size(); first += static_cast<std::size_t>(grid.columns)) {
        std::size_t end = std::min(heights.size(), first + static_cast<std::size_t>(grid.columns));
        float tallest = 0;
        for (std::size_t i = first; i < end; ++i) {
            // Whole pixels: a card at a fraction is resampled, and moved by one it's rebuilt (decision 090's seam).
            float x = std::round(topLeft.x + static_cast<float>(i - first) * (grid.width + gap));
            frames.push_back({ x, std::round(y), grid.width, heights[i] });
            tallest = std::max(tallest, heights[i]);
        }
        y += tallest + gap;
    }
    return frames;
}

float placePage(const WindowPage& page, float paneWidth, const Source& fields, Ms now, const MeasureText& measure, std::vector<Placed>& out,
                std::optional<Rect>& rule)
{
    const Tokens& t = tokens();
    Vec2 pad = t.windowPagePadding;
    float available = std::max(0.0f, paneWidth - 2 * pad.x);
    float y = pad.y;

    // The header: across the whole pane, a line under it from edge to edge.
    if (!page.header.empty()) {
        Slot header = windowStyled(page.header);
        Rect frame{ pad.x, y, available, 0 };
        frame.height = slotHeight(header, frame, fields, measure);
        placeSlot({ "header", &header, frame }, fields, t.windowNavRadius, now, out, measure);
        y = std::round(y + frame.height + pad.y);
        rule = Rect{ 0, y, paneWidth, 1 };
        y += pad.y;
    }

    // The title (a back arrow before it on a page within a page), a line under
    // it, then, in one column, each section as a group of rows (core/groups.h).
    Slot rows;
    if (page.titled) {
        Row title{ .items = { Text{ .value = page.title, .style = TextStyle::PageTitle } } };
        if (!page.backAction.empty()) title.leading = { Button{ .glyph = Glyph::ChevronLeft, .actionId = page.backAction, .compact = true } };
        rows.push_back(title);
        if (!page.subtitle.empty()) rows.push_back(row({ Text{ .value = page.subtitle, .style = TextStyle::Caption } }));
    }
    if (!page.cards)
        for (const WindowSection& section : page.sections) {
            bool plain = section.titleEdit.empty() && section.titleItems.empty() && section.titleTrailing.empty();
            if (plain || section.rows.empty()) {
                addGroup(rows, section.title, unlisted(section.rows)); // nothing without rows
                continue;
            }
            Row label = titleRow(section);
            label.section = !rows.empty();
            rows.push_back(std::move(label));
            std::size_t first = rows.size();
            addGroup(rows, "", unlisted(section.rows));
            rows[first].section = false; // right under its own label, not a gap away
        }
    if (!rows.empty()) {
        rows = windowStyled(std::move(rows));
        Rect frame{ pad.x, y, std::min(t.windowPageMaxWidth, available), 0 };
        frame.height = slotHeight(rows, frame, fields, measure);
        placeSlot({ "page", &rows, frame }, fields, t.windowNavRadius, now, out, measure);
        y += frame.height;
        if (page.cards && !page.sections.empty()) y += t.sectionGap;
    }
    if (!page.cards) return y + pad.y;

    // The cards, side by side as many as fit (decision 117).
    CardGrid grid = cardGrid(available);
    std::vector<Slot> cards;
    std::vector<float> heights;
    for (const WindowSection& section : page.sections) {
        cards.push_back(cardSlot(section));
        heights.push_back(slotHeight(cards.back(), { 0, 0, grid.width, 0 }, fields, measure));
    }
    std::vector<Rect> frames = cardFrames(grid, heights, { pad.x, y });
    float bottom = y;
    for (std::size_t i = 0; i < cards.size(); ++i) {
        placeSlot({ cardName(page.sections[i], i), &cards[i], frames[i] }, fields, t.windowNavRadius, now, out, measure);
        bottom = std::max(bottom, frames[i].y + frames[i].height);
    }
    return bottom + pad.y;
}

} // namespace cup
