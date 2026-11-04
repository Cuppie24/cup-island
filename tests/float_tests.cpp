// Floating cards (decision 081): a floating card's scene (its size, the ✕,
// the card, its question and toast), the cards on screen, and where one
// opens. The rules of opening, focus and closing are in floating.scenario.

#include "core/float_layout.h"
#include "core/float_placement.h"
#include "core/floating.h"
#include "core/hit_test.h"
#include "core/tokens.h"
#include "expect.h"

namespace cup::test {
namespace {

const Placed* placedKey(const Scene& scene, const std::string& key)
{
    for (const Placed& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

Content notes(std::size_t rows)
{
    Content c{ .label = "Notes", .card = CardLayout::Centred };
    for (std::size_t i = 0; i < rows; ++i) c.expandedCenter.push_back(row({ Text{ "Line " + std::to_string(i) } }));
    c.expandedCenter.push_back(row({ Button{ .glyph = Glyph::Plus, .actionId = "add" } }));
    c.floating = FloatSpec{ .width = FloatWidth::Narrow };
    return c;
}

IslandState floating(const Content& c, Ms now)
{
    IslandState s;
    s = reduce(s, ActivityStarted{ { .id = "notes", .content = c, .providerId = "pad" } }, now);
    return reduce(s, PillClicked{}, now);
}

void sceneTests()
{
    const Tokens& t = tokens();
    Ms now{ 0 };
    IslandState s = floating(notes(2), now);
    expect(s.floats.size() == 1 && mode(s) == Mode::Compact, "the card floats; the island kept its pill");
    Scene scene = floatScene(s, s.floats[0], now);
    expect(scene.floating && scene.size.x == t.floatNarrowWidth, "its width class's width, in the island's look");

    // As tall as its content: more rows, a taller card, up to floatMaxHeight.
    IslandState longer = floating(notes(8), now);
    float taller = floatScene(longer, longer.floats[0], now).size.y;
    expect(taller > scene.size.y, "more rows: taller");
    IslandState huge = floating(notes(200), now);
    expect(floatScene(huge, huge.floats[0], now).size.y == t.floatMaxHeight, "never past floatMaxHeight");
    s = reduce(s, FloatResized{ "notes", { 500, 400 } }, now);
    expect(floatScene(s, s.floats[0], now).size == Vec2{ 500, 400 }, "resized: the user's size");

    const Placed* close = placedKey(scene, kCloseFloatKey);
    expect(close && close->frame.x < scene.size.x && close->frame.x + close->frame.width > scene.size.x && close->frame.y < 0,
           "the ✕ sits on the top-right corner, half outside");
    auto hit = hitTest(scene, { scene.size.x, 0 });
    expect(hit && hit->action.actionId == kCloseFloat, "the ✕ is a button the app handles");

    bool routed = true, any = false;
    for (const Placed& p : scene.elements) {
        if (p.key == kCloseFloatKey) continue;
        any = true;
        routed = routed && p.activityId == "notes" && p.providerId == "pad" && p.frame.y >= 0;
    }
    expect(any && routed, "the card's content is in it, routed to its provider");

    // A question replaces its content; a toast floats at its bottom, not on the island.
    s = reduce(s, DialogShown{ { .providerId = "pad", .activityId = "notes", .title = "Clear?", .confirmAction = "clear" } }, now);
    expect(floatScene(s, s.floats[0], now).label == "Clear?", "a dialog takes the card's place");
    s = reduce(s, DialogClosed{}, now);
    s = reduce(s, ToastShown{ { .activityId = "notes", .text = "Deleted", .actionLabel = "Undo", .actionId = "undo" } }, now);
    Scene toasted = floatScene(s, s.floats[0], now);
    expect(toasted.toast && toasted.toast->frame.y + toasted.toast->frame.height <= toasted.size.y, "the toast is on the card");
    expect(!layout(s, now).toast, "...not on the island");

    // Every card on screen: the island's, then each floating one.
    s = reduce(s, ActivityStarted{ { .id = "media", .content = { .label = "Media", .expandedCenter = { row({ Text{ "x" } }) } },
                                     .providerId = "music" } }, now);
    s = reduce(s, ActivityPromoted{ "media" }, now);
    s = reduce(s, PillClicked{}, now);
    auto cards = shownCards(s);
    expect(cards.size() == 2 && cards[0].activityId == "media" && cards[1].activityId == "notes", "both cards are shown");
    IslandState after = reduce(s, FloatClosed{ "notes" }, now);
    after = reduce(after, ActivitySwitched{ "notes" }, now); // its tab: floating again
    CardChanges changes = cardChanges(cards, shownCards(after));
    expect(changes.closed.size() == 1 && changes.closed[0].activityId == "media" && changes.shown.empty(),
           "the island's card closed; the floating one, closed and opened again in between, was never told");

    // The keys' dialog: the focused floating card's, else the island's card's.
    s = reduce(s, DialogShown{ { .providerId = "pad", .activityId = "notes", .title = "?", .confirmAction = "y" } }, now);
    expect(!focusedDialog(s), "the island has the keys; its card has no question");
    s = reduce(s, FloatFocused{ "notes" }, now);
    expect(focusedDialog(s) && focusedDialog(s)->activityId == "notes", "the focused floating card's question");
}

void placementTests()
{
    const Tokens& t = tokens();
    std::vector<Screen> screens{ { "A", { 0, 0, 1920, 1040 }, 1.5f }, { "B", { 1920, 0, 1280, 984 }, 1 } };
    Rect pill{ 800, 0, 320, 48 };
    Vec2 size{ 400, 300 };

    Rect near = placeFloat({}, size, screens, "A", pill, std::nullopt);
    expect(near.width == 600 && near.height == 450, "scaled by its screen");
    expect(near.x == 660 && near.y == 48 + t.floatIslandGap * 1.5f, "under the pill, centred on it");
    Rect centre = placeFloat({ .start = FloatStart::Centre }, size, screens, "A", pill, std::nullopt);
    expect(centre.x == 660 && centre.y == 295, "Centre: in the middle of the work area");
    expect(placeFloat({}, size, screens, "A", { 1800, 0, 100, 40 }, std::nullopt).x + 600 == 1920, "kept on the screen");

    SavedPlace saved = savedPlace({ 2020, 100, 400, 300 }, screens[1]);
    expect(saved == SavedPlace{ "B", { 100, 100, 400, 300 } }, "saved relative to its screen, in logical px");
    expect(placeFloat({}, size, screens, "A", pill, saved) == Rect{ 2020, 100, 400, 300 }, "back where it was left");
    saved.frame.width = 500; // resized by the user
    expect(placeFloat({}, size, screens, "A", pill, saved).width == 400, "not resizable: its own size");
    expect(placeFloat({ .resizable = true }, size, screens, "A", pill, saved).width == 500, "resizable: the size it was left at");
    expect(placeFloat({}, size, { screens[0] }, "A", pill, saved) == near, "its screen gone: placed afresh on the island's");
    Rect huge = placeFloat({ .resizable = true }, size, screens, "A", pill, SavedPlace{ "B", { 1000, 900, 2000, 2000 } });
    expect(huge.x == 1920 && huge.y == 0, "too big for its screen: its top-left corner stays on it");

    // Two cards from one place: the second a step right and down.
    float step = t.floatCascade * 1.5f;
    Rect second = placeFloat({}, size, screens, "A", pill, std::nullopt, { near });
    expect(second.x == near.x + step && second.y == near.y + step, "cascaded off the one already there");
    Rect third = placeFloat({}, size, screens, "A", pill, std::nullopt, { near, second });
    expect(third.x == near.x + 2 * step, "...and the next one further");

    // Remembered per card, else where its provider's last one was.
    Places places;
    remember(places, "pad.notes", "pad", { "B", { 100, 100, 400, 300 } });
    remember(places, "pad.list", "pad", { "A", { 5, 5, 400, 300 } });
    expect(recall(places, "pad.notes", "pad")->screen == "B", "a card's own place");
    expect(recall(places, "pad.new", "pad")->screen == "A", "a card never placed: its provider's last");
    expect(!recall(places, "x.y", "x"), "nothing saved: nothing");
    expect(parsePlaces(formatPlaces(places)) == places, "saved places read back as written");
    Places read = parsePlaces("card\tpad\tB\t1\t2\t3\t4\nbroken line\ncard\tx\tA\tone\t2\t3\t4\nother\tz\tA\t1\t2\t3\t4\n");
    expect(read.byCard.size() == 1 && read.byProvider.empty(), "lines that don't read are skipped");
}

} // namespace

void runFloatTests()
{
    sceneTests();
    placementTests();
}

} // namespace cup::test
