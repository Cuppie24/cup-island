// Providers' windows (decision 090): where one opens, and the registry
// between providers and the open windows. The rules of opening, raising,
// pages and closing are in windows.scenario.

#include "core/item_metrics.h"
#include "core/tokens.h"
#include "core/window_layout.h"
#include "core/window_placement.h"
#include "expect.h"

#include <algorithm>
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
                                         row({ Text{ "Tab row" }, Segmented{ .options = { "Top", "Bottom" }, .actionId = "tabs" } }) } });
    first.sections.push_back({ .title = "More", .rows = { row({ Text{ "Plain" } }) } });
    WindowPage second{ .id = "two", .title = "Two", .backAction = "back", .glyph = Glyph::Sliders };
    WindowSection many;
    for (int i = 0; i < 30; ++i) many.rows.push_back(row({ Text{ "Row " + std::to_string(i) } }));
    second.sections.push_back(many);
    c.pages = { first, second };
    return { .key = { "p", "w" }, .content = c, .page = "one" };
}

const Placed* keyed(const std::vector<Placed>& elements, const std::string& prefix)
{
    for (const Placed& p : elements)
        if (p.key.rfind(prefix, 0) == 0) return &p;
    return nullptr;
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
    const Placed* title = keyed(s.page, "title");
    expect(title && std::get<Text>(title->item).style == TextStyle::PageTitle && title->frame.x == t.windowPagePadding.x &&
           title->frame.y >= t.windowPagePadding.y, "window: the page's title at its padding, in the page's style");
    expect(s.groups.size() == 2 && s.groups[0].x == t.windowPagePadding.x && s.groups[0].width == t.windowPageMaxWidth &&
           s.groups[0].height == 2 * t.windowRowHeight && s.dividers.size() == 1 && s.dividers[0].y == s.groups[0].y + t.windowRowHeight,
           "window: a group per section, its rows at least windowRowHeight, a line between them, no wider than windowPageMaxWidth");
    const Placed* heading = keyed(s.page, "s1h");
    expect(heading && heading->frame.y < s.groups[1].y && heading->frame.y > s.groups[0].y + s.groups[0].height,
           "window: a section's heading between the groups, over its own");
    const Placed* name = keyed(s.page, "s0r0/0/0");
    expect(name && name->frame.x == s.groups[0].x + t.windowRowPadding && std::get<Text>(name->item).detail == "Midnight",
           "window: a row's content windowRowPadding in, its value under its name");

    // What a click means.
    auto at = [&](const Placed& p) { return Vec2{ p.frame.x + s.pane.x + p.frame.width / 2, p.frame.y + s.pane.y + p.frame.height / 2 }; };
    WindowHit hit = windowHitTest(s, at(*name));
    expect(hit.part == WindowPart::Content && hit.hit->action.actionId == "open" && hit.hit->action.windowId == "w" &&
           hit.hit->action.providerId == "p" && hit.hit->action.activityId.empty(),
           "hit: anywhere on a clickAnywhere row is its button, from this window");
    const Placed* seg = keyed(s.page, "s0r1/0/1");
    float half = seg->frame.width / 2;
    WindowHit left = windowHitTest(s, { seg->frame.x + s.pane.x + half / 2, at(*seg).y });
    WindowHit right = windowHitTest(s, { seg->frame.x + s.pane.x + half * 1.5f, at(*seg).y });
    expect(left.hit && left.hit->action.actionId == "tabs" && left.hit->action.value == 0 && right.hit && right.hit->action.value == 1,
           "hit: a segment sends its index");
    expect(seg->frame.width == 2 * segmentWidth(std::get<Segmented>(seg->item), measure) && seg->frame.height == t.segmentedHeight,
           "segmented: equal segments, as wide as the longest label");
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
    const Placed* last = keyed(two.page, "s0r29/0/0");
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

} // namespace

void runWindowTests()
{
    placementTests();
    registryTests();
    layoutTests();
}

} // namespace cup::test
