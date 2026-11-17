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
    // Its menu's "…", last (decision 142): the host opens the menu, the provider never sees this action.
    if (!section.menu.empty())
        r.trailing.push_back(Button{ .glyph = Glyph::More, .actionId = kWindowCardMenu + section.id, .compact = true, .secondary = true });
    return r;
}

bool hasTitle(const WindowSection& section) { return !section.title.empty() || !section.titleTrailing.empty() || !section.menu.empty(); }

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

// What can be dragged in a card (decision 129): its rows with a moveAction, each
// its items' span (their lit row's, if they have one), and the card itself as
// a place rows land if it takes them or has its own.
void recordMovables(const WindowSection& section, const std::string& name, Rect card, const Slot& slot, const std::vector<Placed>& out,
                    std::size_t first, PlacedPage& placed)
{
    std::size_t index = 0;
    for (std::size_t r = 0; r < slot.size(); ++r) {
        const Row& row = slot[r];
        if (row.moveAction.empty()) continue;
        std::string key = name + "/" + (row.id.empty() ? std::to_string(r) : row.id);
        std::optional<Rect> span;
        for (std::size_t i = first; i < out.size(); ++i) {
            if (out[i].key.rfind(key + "/", 0) != 0) continue;
            Rect f = out[i].listRow.value_or(out[i].frame);
            if (!span) span = f;
            float left = std::min(span->x, f.x), top = std::min(span->y, f.y);
            float right = std::max(span->x + span->width, f.x + f.width), bottom = std::max(span->y + span->height, f.y + f.height);
            span = Rect{ left, top, right - left, bottom - top };
        }
        if (span) placed.movables.push_back({ .key = key, .section = section.id, .index = index++, .frame = *span, .action = row.moveAction });
    }
    if (index > 0 || section.takesRows) placed.dropZones.push_back({ section.id, name, card });
}

// A movable card's handle (decision 129): a short line at its top's middle, shown while the
// pointer is on the card; pressed and dragged, the card moves. One line, a little under the
// card's edge (decision 134), in a box lit under the pointer and held anywhere in it (decision 139).
Placed dragHandle(const std::string& name, Rect card, const Source& source)
{
    const Tokens& t = tokens();
    Placed handle;
    handle.key = name + "/handle";
    handle.item = Button{ .glyph = Glyph::Grip, .actionId = kWindowDragHandle, .compact = true, .revealWithSlot = true, .secondary = true };
    // Held where it's lit (decision 139): one box, what shows under the pointer is what can be grabbed.
    float w = std::min(card.width, t.windowDragHandleWidth), h = t.windowDragHandleHeight;
    handle.frame = handle.hitFrame = { std::round(card.x + (card.width - w) / 2), card.y + t.windowDragHandleTop, w, h };
    handle.cornerRadius = h / 2;
    handle.providerId = source.providerId;
    handle.activityId = source.activityId;
    return handle;
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
        for (std::size_t i = first; i < end; ++i) tallest = std::max(tallest, heights[i]);
        for (std::size_t i = first; i < end; ++i) {
            // Whole pixels: a card at a fraction is resampled, and moved by one it's rebuilt (decision 090's seam).
            float x = std::round(left + static_cast<float>(i - first) * (grid.width + gap));
            // As tall as its row's tallest (decision 140): the row's cards end together, its rows still from its top.
            frames.push_back({ x, std::round(y), grid.width, tallest });
        }
        y += tallest + gap;
    }
    return frames;
}

PlacedPage placePage(const WindowPage& page, float paneWidth, const Source& fields, Ms now, const MeasureText& measure, std::vector<Placed>& out,
                     std::optional<Color> accent)
{
    PlacedPage placed;
    std::optional<Rect>& rule = placed.rule;
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
    if (!page.cards) {
        placed.height = y + pad.y;
        return placed;
    }

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
    std::size_t movableCards = 0;
    for (std::size_t i = 0; i < cards.size(); ++i) {
        const WindowSection& section = page.sections[i];
        std::string name = cardName(section, i);
        Rect f = frames[i];
        out.push_back(cardFill(name, f, fields, section.dashed)); // first: under its rows
        Rect rows{ f.x + inside, f.y + inside, std::max(0.0f, f.width - 2 * inside), std::max(0.0f, f.height - 2 * inside) };
        // Its rows' lit backgrounds nest in its corners: its radius less its padding (decision 121).
        std::size_t first = out.size();
        placeSlot({ name, &cards[i], rows }, fields, std::max(0.0f, t.windowCardRadius - inside), now, out, measure);
        bottom = std::max(bottom, f.y + f.height);
        recordMovables(section, name, f, cards[i], out, first, placed);
        if (!section.moveAction.empty()) {
            // Its header picks it up too (decision 143): from its top to its title row's bottom, its whole width.
            Rect grip;
            std::string title = name + "/title/";
            for (std::size_t k = first; k < out.size(); ++k)
                if (out[k].key.rfind(title, 0) == 0) {
                    Rect r = out[k].listRow.value_or(out[k].hitFrame);
                    grip = { f.x, f.y, f.width, std::max(grip.height, r.y + r.height - f.y) };
                }
            placed.movables.push_back({ .card = true, .key = name, .section = section.id, .index = movableCards++, .frame = f, .action = section.moveAction,
                                        .grip = grip });
            out.push_back(dragHandle(name, f, fields));
        }
    }
    placed.height = bottom + pad.y;
    return placed;
}

} // namespace cup
