// Floating windows (decision 081): a window's scene (title bar, card, its
// question and toast), the cards on screen, and where a window opens. The
// rules of opening, focus and closing are in floating.scenario.

#include "core/floating.h"
#include "core/hit_test.h"
#include "core/tokens.h"
#include "core/window_layout.h"
#include "core/window_placement.h"
#include "expect.h"

namespace cup::test {
namespace {

const Placed* placedKey(const Scene& scene, const std::string& key)
{
    for (const Placed& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

IslandState withWindow(Ms now)
{
    Content c{ .label = "Notes", .card = CardLayout::Centred };
    c.expandedCenter = { row({ Text{ "Hello" } }), row({ Button{ .glyph = Glyph::Plus, .actionId = "add" } }) };
    c.window = WindowSpec{};
    IslandState s;
    s = reduce(s, ActivityStarted{ { .id = "notes", .content = c, .providerId = "pad" } }, now);
    s = reduce(s, PillClicked{}, now);
    return s;
}

void sceneTests()
{
    const Tokens& t = tokens();
    Ms now{ 0 };
    IslandState s = withWindow(now);
    expect(s.windows.size() == 1 && mode(s) == Mode::Compact, "the card opened as a window; the island kept its pill");
    Scene scene = windowScene(s, s.windows[0], now);
    expect(scene.size == t.windowMediumSize, "the window is its size class's size");
    expect(scene.titleBar == Rect{ 0, 0, t.windowMediumSize.x, t.windowTitleBarHeight }, "the title bar spans its top");

    const Placed* title = placedKey(scene, "window/title");
    const Placed* close = placedKey(scene, "window/close");
    expect(title && std::get<Text>(title->item).value == "Notes", "the title is the card's label");
    expect(close && close->frame.x + close->frame.width <= scene.size.x && close->frame.y >= 0 &&
               close->frame.y + close->frame.height <= t.windowTitleBarHeight,
           "the ✕ sits in the title bar's right end");
    Vec2 onClose{ close->frame.x + close->frame.width / 2, close->frame.y + close->frame.height / 2 };
    auto hit = hitTest(scene, onClose);
    expect(hit && hit->action.actionId == kCloseWindow, "the ✕ is a button the app handles");

    bool under = true, card = false;
    for (const Placed& p : scene.elements) {
        if (p.key.rfind("window/", 0) == 0) continue;
        card = true;
        under = under && p.frame.y >= t.windowTitleBarHeight && p.activityId == "notes" && p.providerId == "pad";
    }
    expect(card && under, "the card's content is under the title bar, routed to its provider");

    // A question over the window replaces its content; a toast floats at its bottom.
    s = reduce(s, DialogShown{ { .providerId = "pad", .activityId = "notes", .title = "Clear?", .confirmAction = "clear" } }, now);
    Scene asked = windowScene(s, s.windows[0], now);
    expect(asked.label == "Clear?" && placedKey(asked, "window/title"), "a dialog takes the card's place, the title bar stays");
    s = reduce(s, DialogClosed{}, now);
    s = reduce(s, ToastShown{ { .activityId = "notes", .text = "Deleted", .actionLabel = "Undo", .actionId = "undo" } }, now);
    Scene toasted = windowScene(s, s.windows[0], now);
    expect(toasted.toast && toasted.toast->frame.y + toasted.toast->frame.height <= toasted.size.y, "the toast is in the window");
    expect(!layout(s, now).toast, "...not on the island");

    // Every card on screen: the island's, then each window's.
    s = reduce(s, ActivityStarted{ { .id = "media", .content = { .label = "Media", .expandedCenter = { row({ Text{ "x" } }) } },
                                     .providerId = "music" } }, now);
    s = reduce(s, ActivityPromoted{ "media" }, now);
    s = reduce(s, PillClicked{}, now);
    auto cards = shownCards(s);
    expect(cards.size() == 2 && cards[0].activityId == "media" && cards[1].activityId == "notes",
           "the island's card and the window's are both shown");
    IslandState closed = reduce(s, WindowClosed{ "notes" }, now);
    closed = reduce(closed, ActivitySwitched{ "notes" }, now); // its tab: a window again
    CardChanges changes = cardChanges(cards, shownCards(closed));
    expect(changes.closed.size() == 1 && changes.closed[0].activityId == "media" && changes.shown.empty(),
           "the island's card closed; the window's, closed and opened again in between, was never told");

    // The keys' dialog: the focused window's, else the island's card's.
    s = reduce(s, DialogShown{ { .providerId = "pad", .activityId = "notes", .title = "?", .confirmAction = "y" } }, now);
    expect(!focusedDialog(s), "the island has the keys; its card has no question");
    s = reduce(s, WindowFocused{ "notes" }, now);
    expect(focusedDialog(s) && focusedDialog(s)->activityId == "notes", "the focused window's question");
}

void placementTests()
{
    const Tokens& t = tokens();
    std::vector<Screen> screens{ { "A", { 0, 0, 1920, 1040 }, 1.5f }, { "B", { 1920, 0, 1280, 984 }, 1 } };
    Rect pill{ 800, 0, 320, 48 };
    Vec2 size{ 400, 300 };

    Rect near = placeWindow({}, size, screens, "A", pill, std::nullopt);
    expect(near.width == 600 && near.height == 450, "scaled by its screen");
    expect(near.x == 660 && near.y == 48 + t.windowIslandGap * 1.5f, "under the pill, centred on it");

    Rect centre = placeWindow({ .start = WindowStart::Centre }, size, screens, "A", pill, std::nullopt);
    expect(centre.x == 660 && centre.y == 295, "Centre: in the middle of the work area");

    Rect edge = placeWindow({}, size, screens, "A", { 1800, 0, 100, 40 }, std::nullopt);
    expect(edge.x + edge.width == 1920, "kept on the screen");

    SavedPlace saved = savedPlace({ 2020, 100, 400, 300 }, screens[1]);
    expect(saved == SavedPlace{ "B", { 100, 100, 400, 300 } }, "saved relative to its screen, in logical px");
    Rect back = placeWindow({}, size, screens, "A", pill, saved);
    expect(back == Rect{ 2020, 100, 400, 300 }, "back where it was left, on that screen");

    saved.frame.width = 500; // resized by the user
    expect(placeWindow({}, size, screens, "A", pill, saved).width == 400, "not resizable: its size class's size");
    expect(placeWindow({ .resizable = true }, size, screens, "A", pill, saved).width == 500, "resizable: the size it was left at");

    std::vector<Screen> one{ screens[0] };
    expect(placeWindow({}, size, one, "A", pill, saved) == near, "its screen gone: placed afresh on the island's");

    Rect huge = placeWindow({ .resizable = true }, size, screens, "A", pill, SavedPlace{ "B", { 1000, 900, 2000, 2000 } });
    expect(huge.x == 1920 && huge.y == 0, "too big for its screen: the title bar's corner stays on it");

    std::map<std::string, SavedPlace> places{ { "pad", { "B", { 100, 100, 400, 300 } } }, { "clock", { "A", { 1.5f, 2, 3, 4 } } } };
    expect(parsePlaces(formatPlaces(places)) == places, "saved places read back as written");
    auto read = parsePlaces("pad\tB\t1\t2\t3\t4\nbroken line\nx\tA\tone\t2\t3\t4\nz\tA\t1\t2\t0\t4\n");
    expect(read.size() == 1 && read.count("pad"), "lines that don't read are skipped");
}

} // namespace

void runWindowTests()
{
    sceneTests();
    placementTests();
}

} // namespace cup::test
