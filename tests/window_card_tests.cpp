// Window pages of cards, the rail and the header (decision 117): cards one
// width whatever the window's, so a resize moves them and never resizes one.

#include "core/dialog.h"
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
    expect(frames.size() == 3 && frames[1].x == 110 + t.windowCardGap && frames[1].y == 20 && frames[2].x == std::round(10 + (100 + t.windowCardGap) / 2) &&
               frames[2].y == 20 + 80 + t.windowCardGap,
           "grid: row by row, each row as tall as its tallest card; one short of cards centred (decision 123)");
}

void boardTests()
{
    const Tokens& t = tokens();
    auto measure = [](const Text& x) { return 8.0f * static_cast<float>(x.value.size()); };
    OpenWindow w = board();
    WindowScene s = windowLayout(w, { 1500, 800 }, 0, Ms{ 0 }, measure);
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
    WindowScene wider = windowLayout(w, { 1800, 800 }, 0, Ms{ 0 }, measure);
    const Placed* widerFill = nullptr;
    for (const Placed& p : wider.page)
        if (p.key == "card.g0/fill") widerFill = &p;
    expect(widerFill && widerFill->frame.width > fill->frame.width, "wider: the cards grow with the window");
    expect(std::abs(text(wider, "Group 0")->frame.width - first->frame.width) < 0.01f, "...a card's name hugs its text (decision 123)");
    expect(first->frame.width >= measure(std::get<Text>(first->item)), "...all of it: its edit inset isn't taken from its text (decision 124)");
    // The row under the pointer (decision 120): a card's title row, from its name or the space beside it; nothing in the gap.
    auto inWindow = [&](const Placed& p, float dx) { return Vec2{ p.frame.x + s.pane.x + dx, p.frame.y + s.pane.y - s.scroll + p.frame.height / 2 }; };
    expect(windowRowAt(s, inWindow(*first, 2)) == "card.g0/title", "hover: on a card's name, its title row");
    const Placed* deleteButton = nullptr;
    for (const Placed& p : s.page)
        if (p.key.rfind("card.g0/title/", 0) == 0 && std::holds_alternative<Button>(p.item)) deleteButton = &p;
    expect(deleteButton && windowRowAt(s, inWindow(*first, (deleteButton->frame.x - first->frame.x) - 6)).rfind("card.g0/", 0) == 0,
           "hover: between the name and its delete, still that card's row");
    expect(windowRowAt(s, inWindow(*second, 2)) == "card.g1/title", "hover: the card beside it, its own row");
    // Beside the lone card in the last row (5 cards, 2 to a row), level with its name: no card's row (decision 122).
    const Placed* lone = text(s, "Group 4");
    expect(lone && cardGrid(s.pane.width - 2 * t.windowPagePadding.x).columns == 2 &&
               windowRowAt(s, { s.pane.x + second->frame.x + 10, s.pane.y + lone->frame.y - s.scroll + 2 }).empty(),
           "hover: beside the cards, no row");
    expect(windowRowAt(s, { s.pane.x + 2, s.pane.y + 2 }).empty() || windowRowAt(s, { s.pane.x + 2, s.pane.y + 2 }).rfind("header", 0) == 0,
           "hover: the page's margin, no card's row");
    WindowScene narrow = windowLayout(w, { 500, 800 }, 0, Ms{ 0 }, measure);
    const Placed* a = text(narrow, "Group 0");
    const Placed* b = text(narrow, "Group 1");
    expect(a && b && b->frame.y > a->frame.y && b->frame.x == a->frame.x, "narrow: one column");
}

// A window's toast (decision 121): the registry's, over the page's bottom, gone by itself or when answered.
void toastTests()
{
    const Tokens& t = tokens();
    auto measure = [](const Text& x) { return 8.0f * static_cast<float>(x.value.size()); };
    Windows windows;
    WindowContent content{ .title = "T", .pages = { { .id = "one", .title = "One" } } };
    windows.open.push_back({ .key = { "p", "w" }, .content = content, .page = "one" });
    expect(!showWindowToast(windows, { "p", "nope" }, { .text = "x" }, Ms{ 0 }), "toast: no such window, nothing");
    expect(showWindowToast(windows, { "p", "w" }, { .text = "Task deleted", .actionLabel = "Undo", .actionId = "undo", .windowId = "w" }, Ms{ 1000 }) &&
               windows.open[0].toast->until == Ms{ 1000 } + t.toastDuration && nextWindowToastEnd(windows) == Ms{ 1000 } + t.toastDuration,
           "toast: shown until toastDuration from now");
    WindowScene s = windowLayout(windows.open[0], { 900, 600 }, 0, Ms{ 1000 }, measure);
    expect(s.toast && s.toast->frame.y + s.toast->frame.height <= s.pane.height && std::abs(s.toast->frame.x + s.toast->frame.width / 2 - s.pane.width / 2) < 1,
           "toast: a pill centred over the page's bottom");
    expect(windowDeadline(s) == Ms{ 1000 } + t.toastDuration, "toast: the window's timer wakes when it goes");
    const Placed* button = nullptr;
    for (const Placed& p : s.toast->elements)
        if (std::holds_alternative<Button>(p.item)) button = &p;
    WindowHit hit = button ? windowHitTest(s, { s.pane.x + button->frame.x + 3, s.pane.y + button->frame.y + 3 }) : WindowHit{};
    expect(hit.part == WindowPart::Content && hit.hit->action.actionId == "undo" && hit.hit->action.windowId == "w", "toast: its button, from this window");
    expect(!answerWindowToast(windows, { "p", "w" }, "other") && answerWindowToast(windows, { "p", "w" }, "undo") && !windows.open[0].toast,
           "toast: answered, gone");
    showWindowToast(windows, { "p", "w" }, { .text = "again" }, Ms{ 0 });
    expect(!expireWindowToasts(windows, t.toastDuration - Ms{ 1 }) && expireWindowToasts(windows, t.toastDuration) && !windows.open[0].toast,
           "toast: gone when its time is up");

    // A time label that plays: the window's timer wakes for its next second.
    WindowContent timed = content;
    timed.pages[0].header = { row({ TimeLabel{ .timeline = { .duration = Ms{ 60000 }, .playing = true }, .countdown = true } }) };
    OpenWindow ticking{ .key = { "p", "w" }, .content = timed, .page = "one" };
    auto next = windowDeadline(windowLayout(ticking, { 900, 600 }, 0, Ms{ 500 }, measure));
    expect(next && *next > Ms{ 500 } && *next <= Ms{ 1000 }, "a playing time on the page: its next second wakes the window");
    timed.pages[0].header = { row({ TimeLabel{ .timeline = { .duration = Ms{ 60000 } }, .countdown = true } }) };
    ticking.content = timed;
    expect(!windowDeadline(windowLayout(ticking, { 900, 600 }, 0, Ms{ 500 }, measure)), "paused: nothing to wake for");
}

// A window's question (decision 122): a scrim under the title bar, a panel in the middle; only its buttons click.
void dialogTests()
{
    auto measure = [](const Text& x) { return 8.0f * static_cast<float>(x.value.size()); };
    Windows windows;
    WindowContent content{ .title = "T", .pages = { { .id = "one", .title = "One", .sections = { { .rows = { row({ Button{ .actionId = "under", .label = "Under" } }) } } } } } };
    windows.open.push_back({ .key = { "p", "w" }, .content = content, .page = "one" });
    expect(showWindowDialog(windows, { "p", "w" }, { .title = "Delete it?", .message = "Its 2 tasks go with it.", .confirmLabel = "Delete",
                                                     .confirmAction = "gone", .destructive = true, .windowId = "w" }),
           "question: shown over its window");
    WindowScene s = windowLayout(windows.open[0], { 900, 600 }, 0, Ms{ 0 }, measure);
    expect(s.dialog && s.dialog->scrim.y == s.titleBar.height && s.dialog->scrim.height == 600 - s.titleBar.height &&
               std::abs(s.dialog->panel.x + s.dialog->panel.width / 2 - 450) < 1,
           "question: the window dimmed under its title bar, a panel in the middle");
    const Placed *confirm = nullptr, *under = nullptr;
    for (const Placed& p : s.dialog->elements)
        if (auto* b = std::get_if<Button>(&p.item); b && b->actionId == kDialogConfirm) confirm = &p;
    for (const Placed& p : s.page)
        if (auto* b = std::get_if<Button>(&p.item); b && b->actionId == "under") under = &p;
    WindowHit yes = confirm ? windowHitTest(s, { confirm->frame.x + 3, confirm->frame.y + 3 }) : WindowHit{};
    expect(yes.part == WindowPart::Content && yes.hit->action.actionId == kDialogConfirm && yes.hit->action.windowId == "w", "question: its confirm button");
    expect(under && windowHitTest(s, { s.pane.x + under->frame.x + 3, s.pane.y + under->frame.y + 3 }).part == WindowPart::None,
           "question: nothing under it takes a click");
    expect(windowHitTest(s, { 10, 10 }).part == WindowPart::Caption, "question: the title bar still drags");
    expect(answerWindowDialog(windows, { "p", "w" }, true) == std::optional<std::string>("gone") && !windows.open[0].dialog,
           "question: confirmed, its action; gone");
    showWindowDialog(windows, { "p", "w" }, { .title = "Again?", .confirmAction = "gone", .windowId = "w" });
    expect(!answerWindowDialog(windows, { "p", "w" }, false) && !windows.open[0].dialog, "question: cancelled, no action; gone");
}

} // namespace

void runWindowCardTests()
{
    gridTests();
    boardTests();
    toastTests();
    dialogTests();
}

} // namespace cup::test
