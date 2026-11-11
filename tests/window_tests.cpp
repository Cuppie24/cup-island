// Providers' windows (decision 090): where one opens, and the registry
// between providers and the open windows. The rules of opening, raising,
// pages and closing are in windows.scenario.

#include "core/tokens.h"
#include "core/window_placement.h"
#include "expect.h"
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

} // namespace

void runWindowTests()
{
    placementTests();
    registryTests();
}

} // namespace cup::test
