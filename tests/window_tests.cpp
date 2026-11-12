// Providers' windows (decision 104): where one opens, and the registry
// between providers and the open windows. The rules of opening, raising,
// pages and closing are in windows.scenario.

#include "core/segments.h"
#include "core/tokens.h"
#include "core/window_keys.h"
#include "core/window_layout.h"
#include "core/window_placement.h"
#include "expect.h"

#include <algorithm>
#include <cmath>
#include "providers/provider_registry.h"

namespace cup::test {
namespace {

void placementTests()
{
    const Tokens& t = tokens();
    std::vector<Screen> screens{ { "A", { 0, 0, 1920, 1040 }, 1.5f }, { "B", { 1920, 0, 1280, 984 }, 1 } };
    Rect pill{ 800, 0, 320, 48 };
    Vec2 small = t.windowSmallSize;

    WindowSpec spec{ .id = "w", .size = WindowSize::Small, .start = WindowStart::NearIsland };
    Rect near = placeWindow(spec, screens, "A", pill, std::nullopt);
    expect(near.width == small.x * 1.5f && near.height == small.y * 1.5f, "its size class, scaled by its screen");
    expect(near.x == 960 - near.width / 2 && near.y == 48 + t.windowIslandGap * 1.5f, "NearIsland: under the pill, centred on it");
    WindowSpec centred{ .id = "w", .size = WindowSize::Small };
    Rect centre = placeWindow(centred, screens, "A", pill, std::nullopt);
    expect(centre.x == (1920 - centre.width) / 2 && centre.y == (1040 - centre.height) / 2, "Centre: in the middle of the work area");
    expect(placeWindow(spec, screens, "A", { 1800, 0, 100, 40 }, std::nullopt).x + near.width == 1920, "kept on the screen");
    expect(windowSize(WindowSize::Large) == t.windowLargeSize, "a size class is its token");

    SavedPlace saved = savedPlace({ 2020, 100, 500, 400 }, screens[1]);
    expect(saved == SavedPlace{ "B", { 100, 100, 500, 400 } }, "saved relative to its screen, in logical px");
    expect(placeWindow(spec, screens, "A", pill, saved) == Rect{ 2020, 100, 500, 400 }, "back where it was left, at its size");
    WindowSpec fixed = spec;
    fixed.resizable = false;
    expect(placeWindow(fixed, screens, "A", pill, saved).width == small.x, "not resizable: its size class's size");
    expect(placeWindow(spec, screens, "A", pill, SavedPlace{ "B", { 0, 0, 10, 10 } }).width == t.windowMinSize.x,
           "a saved size under windowMinSize: the minimum");
    expect(placeWindow(spec, { screens[0] }, "A", pill, saved) == near, "its screen gone: placed afresh on the island's");
    Rect huge = placeWindow(spec, screens, "A", pill, SavedPlace{ "B", { 1000, 900, 2000, 2000 } });
    expect(huge.x == 1920 && huge.y == 0, "too big for its screen: its top-left corner stays on it");

    float step = t.windowCascade * 1.5f;
    Rect second = placeWindow(spec, screens, "A", pill, std::nullopt, { near });
    expect(second.x == near.x + step && second.y == near.y + step, "cascaded off the one already there");

    Places places;
    remember(places, "pad/notes", "pad", { "B", { 100, 100, 400, 300 } });
    remember(places, "pad/list", "pad", { "A", { 5, 5, 400, 300 } });
    expect(recall(places, "pad/notes", "pad")->screen == "B", "a window's own place");
    expect(recall(places, "pad/new", "pad")->screen == "A", "a window never placed: its provider's last");
    expect(!recall(places, "x/y", "x"), "nothing saved: nothing");
    expect(parsePlaces(formatPlaces(places)) == places, "saved places read back as written");
    Places read = parsePlaces("window\tpad/a\tB\t1\t2\t3\t4\nbroken line\nwindow\tx\tA\tone\t2\t3\t4\nother\tz\tA\t1\t2\t3\t4\n");
    expect(read.byWindow.size() == 1 && read.byProvider.empty(), "lines that don't read are skipped");
}

// A provider that opens its window when its button is clicked, and remembers
// what the host told it.
class Opener : public Provider {
public:
    const char* id() const override { return "opener"; }
    void start(Host& host) override { host_ = &host; }
    void stop() override { host_ = nullptr; }
    void onAction(const Action& a) override
    {
        actions.push_back(a);
        if (a.actionId == "open") opened = host_->openWindow({ .id = "full", .page = a.text }, content("one"));
    }
    void onWindowClosed(const std::string& windowId) override { closed.push_back(windowId); }

    static WindowContent content(const std::string& text)
    {
        WindowContent c{ .title = "Opener" };
        c.pages.push_back({ .id = "main", .sections = { { .rows = { row({ Text{ text } }) } } } });
        c.pages.push_back({ .id = "more" });
        return c;
    }

    Host* host_ = nullptr;
    WindowOpen opened = WindowOpen::NotNow;
    std::vector<Action> actions;
    std::vector<std::string> closed;
};

void registryTests()
{
    Ms now{ 0 };
    int changes = 0;
    ProviderRegistry registry{ [](const Event&, const char*) {}, [&] { return now; } };
    registry.onWindowsChanged([&] { ++changes; });
    auto owned = std::make_unique<Opener>();
    Opener& opener = *owned;
    registry.add(std::move(owned));
    registry.start("opener");

    expect(opener.host_->openWindow({ .id = "full" }, Opener::content("x")) == WindowOpen::NotNow && changes == 0,
           "a window by itself, with no click first: refused");
    registry.route({ "opener", "card", "open" });
    expect(opener.opened == WindowOpen::Opened && registry.windows().open.size() == 1 && changes == 1, "a click on its card opens it");
    const OpenWindow& open = registry.windows().open[0];
    expect(open.key == WindowKey{ "opener", "full" } && open.page == "main", "its first page shown");

    now += Ms{ 60000 };
    opener.host_->updateWindow("full", Opener::content("two"));
    expect(changes == 2 && registry.windows().open[0].content == Opener::content("two"), "new content, any time, without the user");

    registry.windowPageShown({ "opener", "full" }, "more");
    expect(registry.windows().open[0].page == "more", "the user picks a page");
    const Action& paged = opener.actions.back();
    expect(paged.actionId == kWindowPage && paged.text == "more" && paged.windowId == "full" && paged.activityId.empty(),
           "...and its provider is told, from that window");
    registry.route({ "opener", {}, "something", 0, {}, "full" });
    expect(opener.actions.back().windowId == "full", "a window's own buttons reach its provider");

    registry.windowClosed({ "opener", "full" });
    expect(registry.windows().open.empty() && opener.closed == std::vector<std::string>{ "full" }, "the user closes it: its provider is told");
    std::size_t before = opener.actions.size();
    registry.route({ "opener", {}, "late", 0, {}, "full" });
    expect(opener.actions.size() == before, "a click from a window that's gone goes nowhere");

    registry.route({ "opener", "card", "open", 0, "more" });
    expect(registry.windows().open.size() == 1 && registry.windows().open[0].page == "more", "opened at the page asked for");
    opener.host_->closeWindow("full");
    expect(registry.windows().open.empty() && opener.closed.size() == 1, "closed by its provider: not told back");

    registry.route({ "opener", "card", "open" });
    registry.stop("opener");
    expect(registry.windows().open.empty(), "a provider stopping takes its windows with it");
}

// A window of two pages: one group of rows, a segmented choice, and enough
// rows on the second to scroll.
OpenWindow sample()
{
    WindowContent c{ .title = "Sample" };
    WindowPage first{ .id = "one", .title = "One", .subtitle = "About one", .glyph = Glyph::Palette };
    first.sections.push_back({ .rows = { Row{ .items = { Text{ .value = "Theme", .detail = "Midnight" }, Button{ .glyph = Glyph::ChevronRight, .actionId = "open", .compact = true } },
                                              .clickAnywhere = true },
                                         row({ Text{ "Tab row" }, Segments{ .options = { "Top", "Bottom" }, .actionId = "tabs:" } }) } });
    first.sections.push_back({ .title = "More", .rows = { row({ Text{ "Plain" } }) } });
    WindowPage second{ .id = "two", .title = "Two", .backAction = "back", .glyph = Glyph::Sliders };
    WindowSection many;
    for (int i = 0; i < 30; ++i) many.rows.push_back(row({ Text{ "Row " + std::to_string(i) } }));
    second.sections.push_back(many);
    c.pages = { first, second };
    return { .key = { "p", "w" }, .content = c, .page = "one" };
}

void layoutTests()
{
    const Tokens& t = tokens();
    auto measure = [](const Text& text) { return 8.0f * static_cast<float>(text.value.size()); };
    OpenWindow w = sample();
    Vec2 size{ 900, 700 };
    WindowScene s = windowLayout(w, size, 0, Ms{ 0 }, measure);

    expect(s.titleBar == Rect{ 0, 0, 900, t.windowTitleBarHeight } && s.chrome.size() == 2 &&
           s.chrome[1].frame.x + s.chrome[1].frame.width == 900 && s.chrome[0].frame.x + s.chrome[0].frame.width == s.chrome[1].frame.x,
           "window: our own title bar, minimize then close at its right");
    expect(s.sidebar && s.sidebar->width == t.windowSidebarWidth && s.nav.size() == 2 && s.nav[0].picked && !s.nav[1].picked &&
           s.pane.x == t.windowSidebarWidth && s.pane.y == t.windowTitleBarHeight,
           "window: two pages, a sidebar, the shown one picked; the page beside it, under the bar");
    const Placed* title = nullptr;
    for (const Placed& p : s.page)
        if (auto* x = std::get_if<Text>(&p.item); x && x->value == "One") title = &p;
    expect(title && std::get<Text>(title->item).style == TextStyle::PageTitle && title->frame.x == t.windowPagePadding.x &&
           title->frame.y >= t.windowPagePadding.y, "window: the page's title at its padding, in the page's style");
    std::vector<const Placed*> fills, texts;
    for (const Placed& p : s.page) {
        if (std::holds_alternative<GroupFill>(p.item)) fills.push_back(&p);
        if (std::holds_alternative<Text>(p.item)) texts.push_back(&p);
    }
    auto text = [&](const std::string& value) -> const Placed* {
        for (const Placed* p : texts)
            if (std::get<Text>(p->item).value == value) return p;
        return nullptr;
    };
    expect(fills.size() == 3 && fills[0]->frame.x == t.windowPagePadding.x && fills[0]->frame.width == t.windowPageMaxWidth &&
               std::get<GroupFill>(fills[0]->item).roundTop && !std::get<GroupFill>(fills[0]->item).roundBottom &&
               std::abs(fills[1]->frame.y - (fills[0]->frame.y + fills[0]->frame.height)) < 0.01f,
           "window: each section a group of rows on one background (decision 090): its rows' pieces join; no wider than windowPageMaxWidth");
    const Placed* heading = text("More");
    const Placed* name = text("Theme");
    expect(heading && std::get<Text>(heading->item).style == TextStyle::Heading && heading->frame.y > fills[1]->frame.y + fills[1]->frame.height &&
               heading->frame.y < fills[2]->frame.y,
           "window: a section's label between the groups, over its own");
    expect(name && name->frame.x > fills[0]->frame.x && std::get<Text>(name->item).detail == "Midnight" && text("About one")->frame.y > title->frame.y,
           "window: a row's content inside its group, its value under its name; the subtitle under the title");

    // What a click means.
    auto at = [&](const Placed& p) { return Vec2{ p.frame.x + s.pane.x + p.frame.width / 2, p.frame.y + s.pane.y + p.frame.height / 2 }; };
    WindowHit hit = windowHitTest(s, at(*name));
    expect(hit.part == WindowPart::Content && hit.hit->action.actionId == "open" && hit.hit->action.windowId == "w" &&
           hit.hit->action.providerId == "p" && hit.hit->action.activityId.empty(),
           "hit: anywhere on a clickAnywhere row is its button, from this window");
    const Placed* seg = nullptr;
    for (const Placed& p : s.page)
        if (std::holds_alternative<Segments>(p.item)) seg = &p;
    Rect bottom = segmentFrame(std::get<Segments>(seg->item), seg->frame, 1);
    WindowHit right = windowHitTest(s, { bottom.x + s.pane.x + bottom.width / 2, bottom.y + s.pane.y + bottom.height / 2 });
    expect(right.hit && right.hit->action.actionId == "tabs:1" && right.hit->action.windowId == "w", "hit: an option sends its pick, from this window");
    auto middle = [](const Placed& p) { return Vec2{ p.frame.x + p.frame.width / 2, p.frame.y + p.frame.height / 2 }; }; // window coordinates
    expect(windowHitTest(s, { 10, 10 }).part == WindowPart::Caption && windowHitTest(s, middle(s.chrome[1])).part == WindowPart::Close &&
           windowHitTest(s, middle(s.chrome[0])).part == WindowPart::Minimize, "hit: the bar drags; its buttons minimize and close");
    WindowHit nav = windowHitTest(s, { s.nav[1].frame.x + 5, s.nav[1].frame.y + 5 });
    expect(nav.part == WindowPart::Page && nav.page == "two", "hit: a sidebar entry picks its page");
    expect(windowHitTest(s, { s.pane.x + 2, s.pane.y + s.pane.height - 2 }).part == WindowPart::None, "hit: the page's empty space does nothing");

    // The second page scrolls; Esc has its back action.
    w.page = "two";
    WindowScene two = windowLayout(w, size, 1e6f, Ms{ 0 }, measure);
    bool backArrow = std::any_of(two.page.begin(), two.page.end(), [](const Placed& p) {
        auto* b = std::get_if<Button>(&p.item);
        return b && b->actionId == "back" && b->glyph == Glyph::ChevronLeft;
    });
    expect(two.maxScroll > 0 && two.scroll == two.maxScroll && two.backAction == "back" && backArrow,
           "scroll: a long page scrolls, clamped to its end; a page within a page has a back action");
    const Placed* last = nullptr;
    for (const Placed& p : two.page)
        if (auto* x = std::get_if<Text>(&p.item); x && x->value == "Row 29") last = &p;
    Vec2 shown{ last->frame.x + s.pane.x + 2, last->frame.y - two.scroll + two.pane.y + 2 };
    expect(toPage(two, shown).y == last->frame.y + 2, "scroll: a point in the window is the page's, scrolled");
    WindowScene top = windowLayout(w, size, -50, Ms{ 0 }, measure);
    expect(top.scroll == 0, "scroll: never above the top");

    // One page: no sidebar, the page across the window.
    OpenWindow single = sample();
    single.content.pages.pop_back();
    WindowScene alone = windowLayout(single, size, 0, Ms{ 0 }, measure);
    expect(!alone.sidebar && alone.nav.empty() && alone.pane.x == 0 && alone.pane.width == 900, "window: one page, no sidebar");
}

// The keys in a window (decision 104): the island's rules on the page,
// the sidebar beside it, Tab round all of it, scrolled into view.
void keyTests()
{
    auto measure = [](const Text& text) { return 8.0f * static_cast<float>(text.value.size()); };
    OpenWindow w = sample();
    WindowScene s = windowLayout(w, { 900, 700 }, 0, Ms{ 0 }, measure);
    auto all = windowFocusables(s);
    expect(all.size() == 4 && all[0].id == "nav:one" && all[1].id == "nav:two" && all[2].inPage && all[3].inPage,
           "keys: the sidebar's pages, then the page's stops (Theme's row, Tab row's options)");

    auto first = moveWindowFocus(s, "", Arrow::Down);
    expect(first && first->id == all[2].id, "keys: the first press lands on the page's first stop");
    WindowHit theme = activateFocus(s, *first);
    expect(theme.part == WindowPart::Content && theme.hit->action.actionId == "open" && theme.hit->action.windowId == "w",
           "keys: Enter on a row does what a click on it does, from this window");
    auto tabs = moveWindowFocus(s, first->id, Arrow::Down);
    WindowHit picked = activateFocus(s, *tabs);
    expect(tabs && tabs->id == all[3].id && picked.hit && picked.hit->action.actionId == "tabs:1", "keys: down to the options; Enter picks the next one");
    expect(!moveWindowFocus(s, tabs->id, Arrow::Down), "keys: down from the last stop: nothing (no footer)");

    auto side = moveWindowFocus(s, first->id, Arrow::Left);
    expect(side && side->id == "nav:one", "keys: left from the page's edge: the sidebar, on the page shown");
    auto below = moveWindowFocus(s, side->id, Arrow::Down);
    expect(below && below->id == "nav:two" && !moveWindowFocus(s, below->id, Arrow::Down) && !moveWindowFocus(s, side->id, Arrow::Left),
           "keys: up and down through the pages; nothing past them, nothing left of them");
    WindowHit page = activateFocus(s, *below);
    expect(page.part == WindowPart::Page && page.page == "two", "keys: Enter on a page shows it");
    auto back = moveWindowFocus(s, side->id, Arrow::Right);
    expect(back && back->id == first->id, "keys: right from the sidebar: into the page");
    expect(moveWindowFocus(s, "gone", Arrow::Left)->id == "nav:one", "keys: nothing focused, left: the sidebar");

    expect(stepWindowFocus(s, "", false)->id == "nav:one" && stepWindowFocus(s, all[3].id, false)->id == "nav:one" &&
               stepWindowFocus(s, "nav:one", true)->id == all[3].id && stepWindowFocus(s, "nav:two", false)->id == all[2].id,
           "tab: through the sidebar and the page, round at the ends; shift+tab back");

    // A long page: End's stop is below what shows, and scrolling brings it in.
    w.page = "two";
    for (WindowSection& section : w.content.pages[1].sections)
        for (Row& r : section.rows) r = Row{ .items = { Text{ "x" }, Button{ .glyph = Glyph::Edit, .actionId = "edit", .compact = true } }, .clickAnywhere = true };
    WindowScene longPage = windowLayout(w, { 900, 500 }, 0, Ms{ 0 }, measure);
    auto end = pageEndFocus(longPage, true);
    float scroll = end ? scrollToShow(longPage, *end) : 0;
    expect(end && end->pageFrame.y > longPage.pane.height && scroll > 0 && scroll <= longPage.maxScroll &&
               end->pageFrame.y + end->pageFrame.height <= scroll + longPage.pane.height,
           "end: the page's last stop, scrolled into view");
    WindowScene scrolled = windowLayout(w, { 900, 500 }, scroll, Ms{ 0 }, measure);
    auto again = findWindowFocus(scrolled, end->id);
    expect(again && again->frame.y >= scrolled.pane.y && again->frame.y + again->frame.height <= scrolled.pane.y + scrolled.pane.height &&
               scrollToShow(scrolled, *again) == scrolled.scroll,
           "end: once scrolled, it shows inside the pane, and needs no more");
    auto home = pageEndFocus(scrolled, false);
    expect(home && home->id.rfind("el:", 0) == 0 && scrollToShow(scrolled, *home) == 0, "home: the first stop, back at the top");
}

} // namespace

void runWindowTests()
{
    placementTests();
    registryTests();
    layoutTests();
    keyTests();
}

} // namespace cup::test
