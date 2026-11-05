// A row's context menu (decision 086): the host's › on its row, where it
// opens (at the pointer, beside the row, kept on the screen), hit-testing
// its items, and the items' shortcuts.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/menu.h"
#include "core/tokens.h"
#include "expect.h"

namespace cup::test {
namespace {

std::vector<MenuItem> items()
{
    return { { .label = "Open", .actionId = "open", .chord = Chord{ .key = kEnter }, .bold = true },
             { .label = "Run as administrator", .actionId = "admin", .chord = Chord{ .key = kEnter, .ctrl = true, .shift = true } },
             { .label = "Copy", .actionId = "copy", .enabled = false, .group = 1 },
             { .label = "Properties", .actionId = "props", .chord = Chord{ .key = kEnter, .alt = true }, .group = 2 } };
}

// A card with a title row and two rows with menus, shown on the island.
IslandState cardWithMenus(float screen)
{
    Content c{ .label = "card", .card = CardLayout::List, .keepOpen = true };
    c.expandedCenter = { row({ Text{ "Title" } }), Row{ .items = { Text{ "one" } }, .menu = items() },
                         Row{ .items = { Text{ "two" } }, .menu = items() } };
    IslandState s;
    s = reduce(s, ScreenResized{ screen }, Ms{ 0 });
    s = reduce(s, PanelPublished{ { .id = "card", .content = c, .providerId = "p" } }, Ms{ 0 });
    return reduce(s, TileClicked{ "card" }, Ms{ 0 });
}

bool inside(Rect inner, Rect outer)
{
    return inner.x >= outer.x - 0.5f && inner.y >= outer.y - 0.5f && inner.x + inner.width <= outer.x + outer.width + 0.5f &&
           inner.y + inner.height <= outer.y + outer.height + 0.5f;
}

} // namespace

void runMenuTests()
{
    expect(rowKeyOf("expandedCenter/3/1") == "expandedCenter/3", "an element's row: its key less the item");
    expect(chordText(Chord{ .key = kEnter, .ctrl = true, .shift = true }) == "Ctrl+Shift+Enter" &&
               chordText(Chord{ .key = kDelete, .shift = true }) == "Shift+Delete",
           "a menu item's keys read as Windows writes them");

    IslandState s = cardWithMenus(1920);
    Scene scene = layout(s, Ms{ 0 });
    // The host's ›: a row with a menu ends in it; one without, doesn't.
    std::vector<std::string> chevrons;
    for (const Placed& p : scene.elements)
        if (auto* b = std::get_if<Button>(&p.item); b && b->actionId == kMenuOpen) chevrons.push_back(rowKeyOf(p.key));
    expect(chevrons == std::vector<std::string>{ "expandedCenter/1", "expandedCenter/2" }, "rows with a menu end in a ›, the title doesn't");
    auto chevron = std::find_if(scene.elements.begin(), scene.elements.end(), [](const Placed& p) {
        auto* b = std::get_if<Button>(&p.item);
        return b && b->actionId == kMenuOpen;
    });
    auto hit = chevron != scene.elements.end() ? hitTest(scene, { chevron->frame.x + 2, chevron->frame.y + 2 }) : std::nullopt;
    expect(hit && hit->action.actionId == kMenuOpen && rowKeyOf(hit->key) == "expandedCenter/1", "a click on it says which row's menu to open");

    // By the keys: beside the row, top to top, menuGap from its right edge.
    Rect row = rowFrame(scene, "expandedCenter/1");
    expect(row.width > 0 && row.height > 0, "the row is on the card");
    s = reduce(s, MenuOpened{ "card", "expandedCenter/1", std::nullopt }, Ms{ 0 });
    scene = layout(s, Ms{ 0 });
    expect(scene.menu.has_value(), "an open menu is in the scene");
    if (!scene.menu) return;
    const MenuScene& beside = *scene.menu;
    expect(std::abs(beside.frame.x - (row.x + row.width + tokens().menuGap)) < 0.5f && std::abs(beside.frame.y - row.y) < 0.5f,
           "by the keys: beside the row, aligned to its top");
    expect(beside.frame.x + beside.frame.width > scene.size.x, "it reaches past the island's shape");
    Rect bounds = interactiveBounds(scene);
    expect(inside({ scene.offset.x + beside.frame.x, scene.offset.y + beside.frame.y, beside.frame.width, beside.frame.height }, bounds),
           "the interaction window covers it, so it can be clicked");
    expect(beside.items.size() == 4 && beside.lines.size() == 2, "an item each, a line between each two groups");
    expect(beside.lit == std::optional<std::size_t>(0), "its first item lit");
    expect(beside.frame.width >= tokens().menuMinWidth && beside.frame.width <= tokens().menuMaxWidth, "within its widths");
    float labelsEnd = 0, chordsStart = beside.frame.x + beside.frame.width;
    std::size_t labels = 0, chords = 0;
    for (const Placed& p : beside.elements) {
        auto* t = std::get_if<Text>(&p.item);
        if (!t) continue;
        bool chord = t->value.find('+') != std::string::npos || t->value == "Enter";
        (chord ? chords : labels)++;
        if (chord) chordsStart = std::min(chordsStart, p.frame.x);
        else labelsEnd = std::max(labelsEnd, p.frame.x);
        expect(inside(p.frame, beside.frame), "its texts are inside it");
    }
    expect(labels == 4 && chords == 3, "a label per item; a shortcut where it has one");
    expect(chordsStart > labelsEnd, "shortcuts at the right, after the labels");

    // Hit-testing: an item, a dim one, and what's under the menu.
    Rect first = beside.items[0], dim = beside.items[2];
    expect(menuItemAt(beside, { first.x + 4, first.y + first.height / 2 }) == std::optional<std::size_t>(0), "the item under the pointer");
    expect(!menuItemAt(beside, { dim.x + 4, dim.y + dim.height / 2 }), "a dim one can't be chosen");
    expect(!hitTest(scene, { first.x + 4, first.y + first.height / 2 }), "the menu hides what's under it");

    // A right-click: its corner at the pointer; near the screen's right edge, to its left; near the bottom, moved up.
    s = reduce(s, MenuOpened{ "card", "expandedCenter/2", Vec2{ 30, 40 } }, Ms{ 0 });
    scene = layout(s, Ms{ 0 });
    expect(scene.menu && scene.menu->frame.x == 30 && scene.menu->frame.y == 40 && !scene.menu->lit,
           "a right-click: at the pointer, nothing lit");
    IslandState edge = cardWithMenus(1920);
    edge = reduce(edge, SpotRestored{ { .dock = Dock::Right } }, Ms{ 0 });
    Scene atEdge = layout(edge, Ms{ 0 });
    Rect edgeRow = rowFrame(atEdge, "expandedCenter/1");
    edge = reduce(edge, MenuOpened{ "card", "expandedCenter/1", std::nullopt }, Ms{ 0 });
    atEdge = layout(edge, Ms{ 0 });
    expect(atEdge.menu && atEdge.menu->frame.x + atEdge.menu->frame.width <= edgeRow.x - tokens().menuGap + 0.5f,
           "no room on the right of the screen: on the row's left");
    expect(atEdge.menu && atEdge.offset.x + atEdge.menu->frame.x + atEdge.menu->frame.width <= 1920.5f, "never past the screen's edge");
    edge = reduce(edge, MenuOpened{ "card", "expandedCenter/1", Vec2{ atEdge.size.x - 10, tokens().canvas.y - 5 } }, Ms{ 0 });
    atEdge = layout(edge, Ms{ 0 });
    expect(atEdge.menu && atEdge.menu->frame.x + atEdge.menu->frame.width <= atEdge.size.x - 10 + 0.5f, "a right-click near the right edge: to the pointer's left");
    expect(atEdge.menu && atEdge.offset.y + atEdge.menu->frame.y + atEdge.menu->frame.height <= tokens().canvas.y + 0.5f,
           "near the canvas's bottom: moved up into it");

    // Shortcuts: on the lit row, menu open or not; never a dim item's.
    const Content& content = *cardContent(s, "card");
    const MenuItem* admin = itemForChord(content, "expandedCenter/2", Chord{ .key = kEnter, .ctrl = true, .shift = true });
    expect(admin && admin->actionId == "admin", "Ctrl+Shift+Enter: Run as administrator, on that row");
    expect(!itemForChord(content, "expandedCenter/0", Chord{ .key = kEnter, .ctrl = true, .shift = true }), "a row without a menu has no shortcuts");
    expect(!itemForChord(content, "expandedCenter/2", Chord{ .key = 'C', .ctrl = true }), "keys no item has");
}

} // namespace cup::test
