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

// A section's title row: its Heading (edited in place when it says so)
// taking the row's spare width, so a long name isn't cut off and its field
// has room; at its end what goes beside it (a count), then the rest (a delete).
Row titleRow(const WindowSection& section)
{
    Row r{ .items = { Text{ .value = section.title, .style = TextStyle::Heading, .editAction = section.titleEdit,
                            .editMaxLength = section.titleMaxLength } },
           .trailing = section.titleItems };
    for (const Item& item : section.titleTrailing) r.trailing.push_back(item);
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

// A card's rows (decision 120): its title, then its rows, each inset as a
// list item's are; the card is one background around them (cardFill), not a
// group of padded rows, so its rows sit as close as a list's.
Slot cardSlot(const WindowSection& section)
{
    Slot slot;
    if (hasTitle(section)) {
        slot.push_back(titleRow(section));
        slot.back().id = "title"; // one card, one slot: its rows' ids stay unique
    }
    for (Row& r : unlisted(section.rows)) slot.push_back(std::move(r));
    // Each a list item (decisions 121, 122): lit as a whole under the pointer, from its first
    // item (a +, a checkbox) to its last (a delete), listItemPadding round them on every side.
    for (Row& r : slot) {
        r.grouped = false;
        r.listItem = true;
    }
    // Its name hugs its text, what's at its end right beside it, a gap apart; lit only that far (decision 123).
    if (hasTitle(section)) {
        slot.front().hug = true;
        slot.front().edgesBeside = true;
    }
    return windowStyled(std::move(slot));
}

// Its background: one rounded piece behind the whole card, never clicked.
Placed cardFill(const std::string& name, Rect frame, const Source& source, bool dashed)
{
    Placed fill;
    fill.key = name + "/fill";
    fill.item = GroupFill{ .dashed = dashed };
    fill.frame = frame;
    fill.cornerRadius = std::min(tokens().windowCardRadius, frame.width / 2);
    fill.providerId = source.providerId;
    fill.activityId = source.activityId;
    return fill;
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
                else if (auto* field = std::get_if<TextInput>(&item)) field->style = windowStyle(field->style); // its text as the rows' (decision 125)
            }
    return slot;
}

CardGrid cardGrid(float available)
{
    const Tokens& t = tokens();
    available = std::max(0.0f, available);
    int columns = std::max(1, static_cast<int>(std::floor((available + t.windowCardGap) / (t.windowCardMinWidth + t.windowCardGap))));
    float width = (available - t.windowCardGap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    return { columns, std::floor(width) };
}

std::vector<Rect> cardFrames(const CardGrid& grid, const std::vector<float>& heights, Vec2 topLeft)
{
    float gap = tokens().windowCardGap;
    std::vector<Rect> frames;
    float y = topLeft.y;
    for (std::size_t first = 0; first < heights.size(); first += static_cast<std::size_t>(grid.columns)) {
        std::size_t end = std::min(heights.size(), first + static_cast<std::size_t>(grid.columns));
        float tallest = 0;
        // A row short of cards (two groups on a wide screen) sits in the middle, not at the left (decision 123).
        float missing = static_cast<float>(first + static_cast<std::size_t>(grid.columns) - end);
        float left = topLeft.x + missing * (grid.width + gap) / 2;
        for (std::size_t i = first; i < end; ++i) {
            // Whole pixels: a card at a fraction is resampled, and moved by one it's rebuilt (decision 090's seam).
            float x = std::round(left + static_cast<float>(i - first) * (grid.width + gap));
            frames.push_back({ x, std::round(y), grid.width, heights[i] });
            tallest = std::max(tallest, heights[i]);
        }
        y += tallest + gap;
    }
    return frames;
}

float placePage(const WindowPage& page, float paneWidth, const Source& fields, Ms now, const MeasureText& measure, std::vector<Placed>& out,
                std::optional<Rect>& rule, std::optional<Color> accent)
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
        if (page.headerProgress) { // the line is the phase's bar, edge to edge (decision 122)
            float h = t.windowHeaderBarHeight;
            Placed bar;
            bar.key = "header/progress";
            bar.item = Progress{ .timeline = *page.headerProgress };
            bar.frame = { 0, y, paneWidth, h };
            bar.accent = accent;
            bar.providerId = fields.providerId;
            bar.activityId = fields.activityId;
            out.push_back(std::move(bar));
            y += h;
        } else {
            rule = Rect{ 0, y, paneWidth, 1 };
        }
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

    // The cards, side by side as many as fit, filling the row (decisions 117, 120): each a
    // background with its rows windowCardPadding inside it.
    CardGrid grid = cardGrid(available);
    float inside = t.windowCardPadding;
    std::vector<Slot> cards;
    std::vector<float> heights;
    for (const WindowSection& section : page.sections) {
        cards.push_back(cardSlot(section));
        heights.push_back(slotHeight(cards.back(), { 0, 0, std::max(0.0f, grid.width - 2 * inside), 0 }, fields, measure) + 2 * inside);
    }
    std::vector<Rect> frames = cardFrames(grid, heights, { pad.x, y });
    float bottom = y;
    for (std::size_t i = 0; i < cards.size(); ++i) {
        std::string name = cardName(page.sections[i], i);
        Rect f = frames[i];
        out.push_back(cardFill(name, f, fields, page.sections[i].dashed)); // first: under its rows
        Rect rows{ f.x + inside, f.y + inside, std::max(0.0f, f.width - 2 * inside), std::max(0.0f, f.height - 2 * inside) };
        // Its rows' lit backgrounds nest in its corners: its radius less its padding (decision 121).
        placeSlot({ name, &cards[i], rows }, fields, std::max(0.0f, t.windowCardRadius - inside), now, out, measure);
        bottom = std::max(bottom, f.y + f.height);
    }
    return bottom + pad.y;
}

} // namespace cup
