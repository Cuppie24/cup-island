// Grids of cells and Large cards (decision 063): columns from the width,
// cells wrapped into lines, one hover group per cell, actions in its corner,
// a reveal naming a cell, a list filling a Large card, image roles.

#include "core/grid_layout.h"
#include "core/item_metrics.h"
#include "core/menu_layout.h"
#include "core/hit_test.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"

#include <cmath>

namespace cup::test {
namespace {

Cell cellNamed(const std::string& id)
{
    return { .id = id,
             .rows = { row({ Image{ .role = ImageRole::Thumbnail } }), row({ Text{ .value = id, .editAction = "rename." + id, .editHint = true } }) },
             .actions = { Button{ .glyph = Glyph::Delete, .actionId = "remove." + id, .compact = true, .revealOnHover = true } },
             .openAction = "open." + id };
}

IslandState shelfWith(std::size_t cells)
{
    Content c{ .label = "Shelf", .card = CardLayout::Large, .keepOpen = true };
    c.expandedCenter.push_back(row({ Text{ .value = "Shelf", .style = TextStyle::Heading } }));
    Row grid{ .list = "items", .id = "grid", .selectable = true };
    for (std::size_t i = 0; i < cells; ++i) grid.cells.push_back(cellNamed("c" + std::to_string(i)));
    c.expandedCenter.push_back(grid);
    IslandState s;
    s = reduce(s, PanelPublished{ { .id = "shelf", .content = c, .providerId = "shelf" } }, Ms{ 0 });
    return reduce(s, TileClicked{ "shelf" }, Ms{ 0 });
}

const Placed* find(const Scene& scene, const std::string& key)
{
    for (const Placed& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

} // namespace

void runGridTests()
{
    const Tokens& t = tokens();
    expect(gridColumns(t.gridCellMinWidth) == 1 && gridColumns(0) == 1, "grid: at least one column");
    expect(gridColumns(3 * t.gridCellMinWidth + 2 * t.gridGap) == 3 && gridColumns(3 * t.gridCellMinWidth + 2 * t.gridGap - 1) == 2,
           "grid: as many columns as fit at their least width");

    Slot slot{ Row{ .list = "l", .id = "g", .cells = { cellNamed("a"), cellNamed("b"), cellNamed("c") } } };
    WrappedGrids wrapped = wrapGrids(slot, 2 * t.gridCellMinWidth + t.gridGap);
    expect(wrapped.any && wrapped.rows.size() == 2 && wrapped.rows[0].cells.size() == 2 && wrapped.rows[1].cells.size() == 1,
           "grid: cells wrap into lines");
    expect(wrapped.rows[1].id == "g.1" && wrapped.firstCell[1] == 2 && wrapped.columns["l"] == 2 && wrapped.rows[1].list == "l",
           "grid: lines are named, know their first cell, and stay in the list");

    IslandState s = shelfWith(30);
    Scene scene = layout(s, Ms{ 0 });
    expect(scene.mode == Mode::Expanded && scene.size.x == t.largeCardSize.x && scene.size.y >= t.largeCardSize.y, "grid: a Large card is largeCardSize (and its tabs)");
    const Placed* name = find(scene, "expandedCenter/grid.0/c0/1/0");
    const Placed* remove = find(scene, "expandedCenter/grid.0/c0/actions/0");
    const Placed* picture = find(scene, "expandedCenter/grid.0/c0/0/0");
    expect(name && remove && picture, "grid: cell keys are <slot>/<line>/<cell>/<row>/<item>, actions apart");
    if (name && remove && picture) {
        expect(hoverGroup(*name) == "expandedCenter/grid.0/c0" && hoverGroup(*remove) == hoverGroup(*name) && hoverGroup(*picture) == hoverGroup(*name),
               "grid: a cell is one hover group");
        expect(name->listRow && picture->listRow == name->listRow && name->list && name->list->cell == 0u, "grid: lit as a whole; its cell index known");
        expect(name->openAction == "open.c0" && remove->openAction == "open.c0", "grid: a double-click anywhere on it opens it");
        const Rect& cell = *name->listRow;
        expect(remove->frame.x + remove->frame.width <= cell.x + cell.width && remove->frame.y < name->frame.y && remove->frame.y >= cell.y,
               "grid: actions sit in the cell's top-right corner");
        auto hit = hitTest(scene, { remove->frame.x + remove->frame.width / 2, remove->frame.y + remove->frame.height / 2 });
        expect(hit && hit->action.actionId == "remove.c0", "grid: the actions take clicks over the picture");
        auto edit = hitTest(scene, { name->frame.x + name->frame.width / 2, name->frame.y + name->frame.height / 2 });
        expect(edit && edit->action.actionId == "rename.c0", "grid: a click on the name renames");
        expect(picture->frame.height == t.thumbnailHeight, "grid: a Thumbnail is thumbnailHeight tall");
        expect(picture->hitFrame == cell, "grid: the whole cell is under the pointer, for hover");
    }
    std::size_t columns = gridColumns(t.largeCardSize.x - 2 * t.expandedPadding);
    const Placed* second = find(scene, "expandedCenter/grid.1/c" + std::to_string(columns) + "/1/0");
    expect(second && second->list && second->list->row == 1 && second->list->cell == columns, "grid: the next line starts where the first ended");
    // The list fills what the heading leaves of the card.
    if (second && second->list) {
        float bottom = second->list->viewport.y + second->list->viewport.height;
        expect(bottom <= scene.size.y - t.expandedPadding + 0.5f && bottom > scene.size.y - t.expandedPadding - t.gridCellHeight,
               "grid: a list in a Large card fills it");
    }
    // A reveal names a cell: it scrolls to that cell's line.
    s = reduce(s, ListRevealed{ "shelf", "items", 29 }, Ms{ 0 });
    scene = layout(s, Ms{ 0 });
    const Placed* last = find(scene, "expandedCenter/grid." + std::to_string(29 / columns) + "/c29/1/0");
    expect(last && last->list && last->list->offset > 0 && last->frame.y >= last->list->viewport.y, "grid: revealing a cell scrolls to its line");

    // Picked cells (decision 064): known by their ids, lit once each.
    s = reduce(s, SelectionSet{ "shelf", "items", { "c0", "c1" } }, Ms{ 0 });
    scene = layout(s, Ms{ 0 });
    const Placed* picked = find(scene, "expandedCenter/grid.0/c1/1/0");
    expect(picked && picked->list && picked->list->item == "c1", "grid: a selectable grid's cells carry their ids");
    expect(scene.selected.size() == 2 && picked && picked->listRow && scene.selected[1] == *picked->listRow, "grid: each picked cell is lit, once");

    // A cell clickable anywhere (the add card): its button takes the whole cell, lit as one.
    {
        Content c{ .card = CardLayout::Large };
        Row grid{ .list = "items", .id = "grid", .selectable = true };
        grid.cells.push_back({ .id = "add", .rows = { row({ Button{ .glyph = Glyph::Plus, .actionId = "add" } }) }, .fixed = true, .clickAnywhere = true });
        c.expandedCenter.push_back(grid);
        IslandState g;
        g = reduce(g, PanelPublished{ { .id = "p", .content = c, .providerId = "p" } }, Ms{ 0 });
        g = reduce(g, TileClicked{ "p" }, Ms{ 0 });
        Scene gs = layout(g, Ms{ 0 });
        const Placed* plus = find(gs, "expandedCenter/grid.0/add/0/0");
        expect(plus && plus->listRow && plus->hitFrame == *plus->listRow && plus->litByRow && plus->list && plus->list->item.empty(),
               "grid: a clickable cell is one button, lit as a whole, never picked");
    }
    // A name that lights to say it renames: its whole stretch, a field's height.
    {
        const Placed* nm = find(scene, "expandedCenter/grid.0/c1/1/0");
        expect(nm && nm->frame.height == lineHeight(TextStyle::Body) + 2 * t.editInset && nm->listRow && nm->frame.width > nm->listRow->width / 2,
               "grid: a hinted name is a field's height, across the cell");
    }

    // The context menu (decision 066): kept in the card, flipped at its edges, its items first.
    {
        Rect card{ 10, 10, 400, 300 };
        expect(menuFrame({ 50, 50 }, { 200, 100 }, card) == Rect{ 50, 50, 200, 100 }, "menu: at the pointer");
        expect(menuFrame({ 350, 280 }, { 200, 100 }, card) == Rect{ 150, 180, 200, 100 }, "menu: flipped left and up at the card's edges");
        IslandState m = shelfWith(3);
        m = reduce(m, MenuOpened{ { "shelf", "shelf", "items", { MenuItem{ .label = "Open", .actionId = "open" }, MenuItem{ .label = "Remove", .actionId = "remove" } }, { 40, 60 } } }, Ms{ 0 });
        Scene ms = layout(m, Ms{ 0 });
        expect(ms.menu && ms.elements.front().key.rfind("menu/", 0) == 0, "menu: its panel, and its items first");
        const Placed* open = find(ms, "menu/0/0");
        auto hit = open ? hitTest(ms, { ms.menu->frame.x + ms.menu->frame.width - 10, open->frame.y + open->frame.height / 2 }) : std::nullopt;
        expect(hit && hit->action.actionId == "open", "menu: an item takes clicks across its whole row, over the card");
    }

    // A whole-row button in a list item (the list's add row): its click area is its lit background.
    {
        Content c{ .card = CardLayout::Centred,
                   .expandedCenter = { Row{ .items = { Button{ .glyph = Glyph::Plus, .actionId = "add", .label = "Add files", .compact = true } },
                                            .align = Align::Center, .clickAnywhere = true, .id = "add", .listItem = true } } };
        IslandState l;
        l = reduce(l, PanelPublished{ { .id = "p", .content = c, .providerId = "p" } }, Ms{ 0 });
        l = reduce(l, TileClicked{ "p" }, Ms{ 0 });
        Scene ls = layout(l, Ms{ 0 });
        const Placed* add = find(ls, "expandedCenter/add/0");
        expect(add && add->listRow && add->hitFrame.x <= add->listRow->x && add->hitFrame.x + add->hitFrame.width >= add->listRow->x + add->listRow->width,
               "grid: a whole-row button's click area is its row's background, end to end");
    }

    // Image roles in an ordinary row.
    Content listed{ .card = CardLayout::Centred, .expandedCenter = { row({ Image{ .role = ImageRole::Icon }, Text{ .value = "a" } }) } };
    IslandState l;
    l = reduce(l, PanelPublished{ { .id = "p", .content = listed, .providerId = "p" } }, Ms{ 0 });
    l = reduce(l, TileClicked{ "p" }, Ms{ 0 });
    const Placed* icon = find(layout(l, Ms{ 0 }), "expandedCenter/0/0");
    expect(icon && icon->frame.height == t.iconSize && icon->frame.width == t.iconSize, "grid: an Icon image is icon-sized");
}

} // namespace cup::test
