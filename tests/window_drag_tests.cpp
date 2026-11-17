// Dragging on a window's page (decision 129): a row within its card or into
// another, a card among the cards by its handle; the page laid out as if it
// were where it lands, the carried thing following the pointer.

#include "core/tokens.h"
#include "core/window_drag.h"
#include "expect.h"

#include <string>

namespace cup::test {
namespace {

OpenWindow board()
{
    WindowContent c{ .title = "Board" };
    WindowPage page{ .id = "p", .title = "P", .titled = false, .cards = true };
    for (int g = 0; g < 2; ++g) {
        std::string gid = std::to_string(g);
        WindowSection s{ .title = "Group " + gid, .id = "g" + gid, .moveAction = "move.g" + gid, .takesRows = true };
        for (int r = 0; r < 3; ++r) {
            std::string rid = gid + "." + std::to_string(r);
            s.rows.push_back(Row{ .items = { Text{ "Row " + rid } }, .moveAction = "row." + rid, .id = "r" + std::to_string(r) });
        }
        page.sections.push_back(s);
    }
    page.sections.push_back({ .rows = { row({ Text{ "New" } }) }, .id = "new" }); // doesn't move, takes nothing
    c.pages = { page };
    return { .key = { "w", "board" }, .content = c, .page = "p" };
}

auto measure = [](const Text& x) { return 8.0f * static_cast<float>(x.value.size()); };

WindowScene laid(const OpenWindow& w) { return windowLayout(w, { 1400, 900 }, 0, Ms{ 0 }, measure); }

const Movable* movable(const WindowScene& s, const std::string& action)
{
    for (const Movable& m : s.movables)
        if (m.action == action) return &m;
    return nullptr;
}

Vec2 inWindow(const WindowScene& s, Vec2 page) { return { page.x + s.pane.x, page.y + s.pane.y - s.scroll }; }

void dragTests()
{
    OpenWindow w = board();
    WindowScene s = laid(w);
    const Movable* g0 = movable(s, "move.g0");
    const Movable* r00 = movable(s, "row.0.0");
    const Movable* r02 = movable(s, "row.0.2");
    expect(s.movables.size() == 8 && g0 && g0->card && r00 && !r00->card && r00->section == "g0" && r02 && r02->index == 2 && s.dropZones.size() == 2,
           "movables: two cards, three rows each; the cards that take rows; not the New card");
    const Placed* handle = nullptr;
    for (const Placed& p : s.page)
        if (p.key == g0->key + "/handle") handle = &p;
    const Tokens& tk = tokens();
    expect(handle && std::get<Button>(handle->item).revealWithSlot && handle->frame.y == g0->frame.y + tk.windowDragHandleTop &&
               std::abs(handle->frame.x + handle->frame.width / 2 - (g0->frame.x + g0->frame.width / 2)) < 1,
           "a movable card's handle: at its top's middle, a little under its edge, shown with its card");
    expect(handle->hitFrame.y == g0->frame.y && handle->hitFrame.width > handle->frame.width &&
               handle->hitFrame.y + handle->hitFrame.height >= handle->frame.y + handle->frame.height,
           "...held in a wider band than it draws, from the card's very top (decision 134)");
    auto onHandleEdge = inWindow(s, { handle->hitFrame.x + 2, handle->hitFrame.y + 1 });
    expect(movableAt(s, onHandleEdge) && movableAt(s, onHandleEdge)->card, "pressed at the band's edge: the card");
    expect(windowRowAt(s, inWindow(s, { handle->frame.x + 2, handle->frame.y + 2 })).empty() &&
               windowCardAt(s, inWindow(s, { handle->frame.x + 2, handle->frame.y + 2 })) == g0->key,
           "over the handle: its card, and no row of it lit (it lit the card's last one, decision 134)");
    auto picked = movableAt(s, inWindow(s, { handle->frame.x + 2, handle->frame.y + 2 }));
    expect(picked && picked->card && picked->key == g0->key, "pressed on the handle: the card");
    picked = movableAt(s, inWindow(s, { r00->frame.x + 20, r00->frame.y + 2 }));
    expect(picked && picked->action == "row.0.0", "pressed on a row: the row");
    expect(windowCardAt(s, inWindow(s, { g0->frame.x + 5, g0->frame.y + g0->frame.height - 5 })) == g0->key, "the card under the pointer");

    // A row carried down past the next one in its card: it lands after it.
    WindowDrag d = startDrag(s, *r00, inWindow(s, { r00->frame.x + 20, r00->frame.y + 5 }));
    expect(dropTarget(s, d, origin(d)) == origin(d) && !dropAction(s, d, dropTarget(s, d, origin(d))), "not moved: lands where it was, nothing sent");
    const Movable* r01 = movable(s, "row.0.1");
    d.at.y += r01->frame.y + r01->frame.height / 2 - r00->frame.y + 2;
    DropTarget t = dropTarget(s, d, origin(d));
    expect(t.section == "g0" && t.index == 1, "past the next row's middle: after it");
    WindowContent moved = withMove(w.content, "p", d, t);
    expect(moved.pages[0].sections[0].rows[1].moveAction == "row.0.0" && moved.pages[0].sections[0].rows[0].moveAction == "row.0.1",
           "laid out as if it were there: the others make room");
    OpenWindow preview = w;
    preview.content = moved;
    WindowScene shown = laid(preview);
    follow(shown, d);
    const Movable* carried = movable(shown, "row.0.0");
    expect(carried && std::abs(carried->frame.y - (d.at.y - d.grab.y)) < 0.5f, "...and the carried row follows the pointer");
    // Decision 134: only the carried row's items are carried (at the pointer at once); an outline where it lands.
    bool carriedOnly = true;
    const Placed* spot = nullptr;
    for (const Placed& p : shown.page) {
        bool mine = p.key.rfind(carried->key + "/", 0) == 0;
        if (mine != p.carried) carriedOnly = false;
        if (p.key == kDropSpot) spot = &p;
    }
    WindowScene landed = laid(preview);
    const Movable* home = movable(landed, "row.0.0");
    expect(carriedOnly, "carried: the row's own items, and nothing else");
    expect(spot && std::get<GroupFill>(spot->item).outline && home && spot->frame == home->frame && &shown.page.front() == spot,
           "a soft outline where it lands, under everything");
    auto action = dropAction(s, d, t);
    expect(action && action->actionId == "row.0.0" && action->value == 1 && action->text == "g0" && action->windowId == "board",
           "let go: its move action, its new index, its card");

    // Into the other card, near its top: its first place there.
    const Movable* r10 = movable(s, "row.1.0");
    d.at = { r10->frame.x + 20, r10->frame.y + 1 };
    t = dropTarget(s, d, origin(d));
    expect(t.section == "g1" && t.index == 0, "over the other card's first row: its first place");
    moved = withMove(w.content, "p", d, t);
    expect(moved.pages[0].sections[0].rows.size() == 2 && moved.pages[0].sections[1].rows.size() == 4 &&
               moved.pages[0].sections[1].rows[0].moveAction == "row.0.0",
           "laid out in the other card");
    d.at = { 5, 5 }; // over no card
    expect(dropTarget(s, d, origin(d)) == origin(d), "over no card (from where it was): it stays");

    // A card carried past the other: it goes after it.
    const Movable* g1 = movable(s, "move.g1");
    WindowDrag card = startDrag(s, *g0, inWindow(s, { g0->frame.x + g0->frame.width / 2, g0->frame.y + 3 }));
    card.at.x += g1->frame.x + g1->frame.width / 2 - g0->frame.x - g0->frame.width / 2 + 5;
    t = dropTarget(s, card, origin(card));
    expect(t.section.empty() && t.index == 1, "a card past the other's middle: after it");
    moved = withMove(w.content, "p", card, t);
    expect(moved.pages[0].sections[0].id == "g1" && moved.pages[0].sections[1].id == "g0" && moved.pages[0].sections[2].id == "new",
           "laid out after it; the card that doesn't move stays last");
    action = dropAction(s, card, t);
    expect(action && action->actionId == "move.g0" && action->value == 1, "let go: the card's move action, its new index");
}

// Decision 135: a tall card dragged under a small one, one column. The small one moves up when the
// pointer passes its middle; in the room the tall one leaves, it stays up; it goes back down only
// when the pointer is over its top half as it shows now.
void tallCardTests()
{
    OpenWindow w = board();
    auto& sections = w.content.pages[0].sections;
    for (int r = 3; r < 10; ++r) sections[0].rows.push_back(Row{ .items = { Text{ "More " + std::to_string(r) } }, .moveAction = "row.0." + std::to_string(r) });
    Vec2 size{ 700, 3000 }; // one column
    auto lay = [&](const OpenWindow& o) { return windowLayout(o, size, 0, Ms{ 0 }, measure); };
    WindowScene s = lay(w);
    const Movable* tall = movable(s, "move.g0");
    const Movable* small = movable(s, "move.g1");
    expect(tall && small && small->frame.y > tall->frame.y + tall->frame.height - 1 && tall->frame.height > 2 * small->frame.height,
           "one column: the tall card over the small one");
    WindowDrag d = startDrag(s, *tall, inWindow(s, { tall->frame.x + tall->frame.width / 2, tall->frame.y + 4 }));
    DropTarget t = origin(d);
    auto previewAt = [&](float y) {
        d.at.y = y;
        t = dropTarget(s, d, t);
        OpenWindow preview = w;
        preview.content = withMove(w.content, "p", d, t);
        s = lay(preview);
        follow(s, d);
        return t.index;
    };
    float smallTop = small->frame.y, smallHeight = small->frame.height;
    expect(previewAt(smallTop + smallHeight * 0.25f) == 0, "over the small card's top half: still before it");
    expect(previewAt(smallTop + smallHeight * 0.75f) == 1, "past its middle: after it, the small card moves up");
    const Movable* up = movable(s, "move.g1");
    expect(up && up->frame.y < smallTop, "...shown up top now");
    float upTop = up->frame.y, upHeight = up->frame.height;
    bool stays = true;
    for (float y = upTop + upHeight + 2; y < upTop + upHeight + 3 * smallHeight; y += 5) stays = stays && previewAt(y) == 1;
    expect(stays, "the pointer in the room the tall card left: the small one stays up");
    expect(previewAt(upTop + upHeight * 0.75f) == 1, "over its bottom half as shown: still after it");
    expect(previewAt(upTop + upHeight * 0.25f) == 0, "over its top half as shown: before it again, it goes back down");
}

// Decision 136: tasks the same way. A tall (wrapped) task dragged under a short one: the short one
// moves up past its middle, stays up in the room the tall one leaves, and goes back down only over
// its top half as it shows now.
void tallRowTests()
{
    OpenWindow w = board();
    Row& first = w.content.pages[0].sections[0].rows[0];
    std::string words;
    for (int i = 0; i < 60; ++i) words += "word ";
    first.items = { Text{ .value = words, .wrap = true } };
    WindowScene s = laid(w);
    const Movable* tall = movable(s, "row.0.0");
    const Movable* next = movable(s, "row.0.1");
    expect(tall && next && tall->frame.height > 2 * next->frame.height, "the first task wraps: taller than the next");
    WindowDrag d = startDrag(s, *tall, inWindow(s, { tall->frame.x + 20, tall->frame.y + 4 }));
    DropTarget t = origin(d);
    auto previewAt = [&](float y) {
        d.at.y = y;
        t = dropTarget(s, d, t);
        OpenWindow preview = w;
        preview.content = withMove(w.content, "p", d, t);
        s = laid(preview);
        follow(s, d);
        return t;
    };
    float nextTop = next->frame.y, nextHeight = next->frame.height;
    expect(previewAt(nextTop + nextHeight * 0.25f).index == 0, "over the next task's top half: still before it");
    expect(previewAt(nextTop + nextHeight * 0.75f).index == 1, "past its middle: after it, it moves up");
    const Movable* up = movable(s, "row.0.1");
    expect(up && up->frame.y < nextTop, "...shown up top now");
    float upTop = up->frame.y, upHeight = up->frame.height;
    bool stays = true;
    for (float y = upTop + upHeight + 2; y < upTop + upHeight + 2 * nextHeight; y += 4) {
        DropTarget at = previewAt(y);
        stays = stays && at.section == "g0" && at.index == 1;
    }
    expect(stays, "the pointer in the room the tall task left: the short one stays up");
    expect(previewAt(upTop + upHeight * 0.25f).index == 0, "over its top half as shown: before it again");
}

} // namespace

void runWindowDragTests()
{
    dragTests();
    tallCardTests();
    tallRowTests();
}

} // namespace cup::test
