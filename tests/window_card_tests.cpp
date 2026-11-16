// Window pages of cards, the rail and the header (decision 117): cards one
// width whatever the window's, so a resize moves them and never resizes one.

#include "core/tokens.h"
#include "core/window_layout.h"
#include "core/window_page.h"
#include "expect.h"

#include <cmath>
#include <string>

namespace cup::test {
namespace {

OpenWindow board()
{
    WindowContent c{ .title = "Board", .rail = true };
    WindowPage first{ .id = "one", .title = "One", .glyph = Glyph::Timer, .titled = false, .header = { row({ Text{ "Header" } }) }, .cards = true };
    for (int i = 0; i < 5; ++i)
        first.sections.push_back({ .title = "Group " + std::to_string(i), .rows = { row({ Text{ "Row " + std::to_string(i) } }) },
                                   .id = "g" + std::to_string(i), .titleEdit = "rename." + std::to_string(i),
                                   .titleTrailing = { Button{ .glyph = Glyph::Delete, .actionId = "delete", .compact = true, .revealOnHover = true } } });
    WindowPage second{ .id = "two", .title = "Two", .glyph = Glyph::Chart };
    WindowPage foot{ .id = "three", .title = "Three", .glyph = Glyph::Settings, .atFoot = true };
    c.pages = { first, second, foot };
    return { .key = { "p", "w" }, .content = c, .page = "one" };
}

const Placed* text(const WindowScene& s, const std::string& value)
{
    for (const Placed& p : s.page)
        if (auto* x = std::get_if<Text>(&p.item); x && x->value == value) return &p;
    return nullptr;
}

void gridTests()
{
    const Tokens& t = tokens();
    float two = 2 * t.windowCardMinWidth + t.windowCardGap;
    expect(cardGrid(two).columns == 2 && cardGrid(two - 1).columns == 1 && cardGrid(two).width == t.windowCardMinWidth,
           "grid: as many cards to a row as fit at windowCardMinWidth");
    expect(cardGrid(two - 1).width == std::floor(two - 1) && cardGrid(two + 100).width == std::floor(t.windowCardMinWidth + 50),
           "grid: the cards share the row's width (decision 120)");
    auto frames = cardFrames({ 2, 100 }, { 50, 80, 30 }, { 10, 20 });
    expect(frames.size() == 3 && frames[1].x == 110 + t.windowCardGap && frames[1].y == 20 && frames[2].x == 10 &&
               frames[2].y == 20 + 80 + t.windowCardGap,
           "grid: row by row, each row as tall as its tallest card");
}

void boardTests()
{
    const Tokens& t = tokens();
    auto measure = [](const Text& x) { return 8.0f * static_cast<float>(x.value.size()); };
    OpenWindow w = board();
    WindowScene s = windowLayout(w, { 1200, 800 }, 0, Ms{ 0 }, measure);
    expect(s.rail && s.sidebar && s.sidebar->width == t.windowRailWidth && s.nav.size() == 3, "rail: windowRailWidth, an entry per page");
    expect(s.nav[2].frame.y > s.nav[1].frame.y + 100 && s.nav[2].frame.y + s.nav[2].frame.height <= 800, "rail: a page atFoot at its bottom");
    bool title = false;
    for (const Placed& p : s.chrome)
        if (auto* x = std::get_if<Text>(&p.item); x && x->value == "Board") title = true;
    expect(title, "rail: the window's title in its title bar");
    const Placed* header = text(s, "Header");
    const Placed* first = text(s, "Group 0");
    expect(header && s.headerRule && s.headerRule->x == 0 && s.headerRule->width == s.pane.width && s.headerRule->y > header->frame.y &&
               first && first->frame.y > s.headerRule->y && !text(s, "One"),
           "header: across the pane, a line under it, the cards under that; no page title");
    expect(first && std::get<Text>(first->item).editAction == "rename.0" && first->key.rfind("card.g0/", 0) == 0,
           "a card's title inside it, edited in place; its keys named by the section's id");
    const Placed* second = text(s, "Group 1");
    expect(second && second->frame.y == first->frame.y && second->frame.x > first->frame.x, "wide: cards side by side");

    // Each card one background, its rows windowCardPadding inside it (decision 120).
    const Placed* fill = nullptr;
    for (const Placed& p : s.page)
        if (p.key == "card.g0/fill") fill = &p;
    expect(fill && std::holds_alternative<GroupFill>(fill->item) && first->frame.x >= fill->frame.x + t.windowCardPadding &&
               first->frame.y >= fill->frame.y + t.windowCardPadding,
           "a card: one background, its title inside its padding");
    WindowScene wider = windowLayout(w, { 1500, 800 }, 0, Ms{ 0 }, measure);
    const Placed* widerFirst = text(wider, "Group 0");
    expect(widerFirst && widerFirst->frame.width > first->frame.width, "wider: the cards grow with the window");
    // The row under the pointer (decision 120): a card's title row, from its name or the space beside it; nothing in the gap.
    auto inWindow = [&](const Placed& p, float dx) { return Vec2{ p.frame.x + s.pane.x + dx, p.frame.y + s.pane.y - s.scroll + p.frame.height / 2 }; };
    expect(windowRowAt(s, inWindow(*first, 2)) == "card.g0/title", "hover: on a card's name, its title row");
    const Placed* deleteButton = nullptr;
    for (const Placed& p : s.page)
        if (p.key.rfind("card.g0/title/", 0) == 0 && std::holds_alternative<Button>(p.item)) deleteButton = &p;
    expect(deleteButton && windowRowAt(s, inWindow(*first, (deleteButton->frame.x - first->frame.x) - 6)).rfind("card.g0/", 0) == 0,
           "hover: between the name and its delete, still that card's row");
    expect(windowRowAt(s, inWindow(*second, 2)) == "card.g1/title", "hover: the card beside it, its own row");
    expect(windowRowAt(s, { s.pane.x + 2, s.pane.y + 2 }).empty() || windowRowAt(s, { s.pane.x + 2, s.pane.y + 2 }).rfind("header", 0) == 0,
           "hover: the page's margin, no card's row");
    WindowScene narrow = windowLayout(w, { 500, 800 }, 0, Ms{ 0 }, measure);
    const Placed* a = text(narrow, "Group 0");
    const Placed* b = text(narrow, "Group 1");
    expect(a && b && b->frame.y > a->frame.y && b->frame.x == a->frame.x, "narrow: one column");
}

} // namespace

void runWindowCardTests()
{
    gridTests();
    boardTests();
}

} // namespace cup::test
