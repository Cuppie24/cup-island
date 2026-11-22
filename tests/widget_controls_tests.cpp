// A widget's controls (decision 163): a fixed column at its right, the
// primary on the same line on every widget, left out where the text would
// be too narrow; stops for Tab after their widget; not part of the face.

#include "core/hit_test.h"
#include "core/keyboard.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"

#include <cmath>

namespace cup::test {
namespace {

const Placed* find(const Scene& scene, const std::string& key)
{
    for (const auto& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

Button button(const char* action, bool primary = false)
{
    return Button{ .glyph = Glyph::Play, .actionId = action, .compact = !primary, .large = primary };
}

Panel withControls(const std::string& id, TileControls controls, bool wide = true)
{
    Content content{ .label = id, .expandedCenter = { row({ Text{ id } }) }, .tile = { row({ Text{ id } }) }, .tileControls = std::move(controls) };
    return { .id = id, .content = content, .wide = wide, .providerId = "test" };
}

Scene home(std::initializer_list<Panel> panels)
{
    IslandState s;
    for (const Panel& p : panels) s = reduce(s, PanelPublished{ p }, Ms{ 0 });
    return layout(reduce(s, PillClicked{}, Ms{ 0 }), Ms{ 0 });
}

float centreX(const Placed& p) { return p.frame.x + p.frame.width / 2; }

} // namespace

void runWidgetControlsTests()
{
    const Tokens& t = tokens();
    float column = 2 * t.smallButtonSize + t.largeButtonSize + 2 * t.tileControlGap;

    // Two wide widgets side by side: each has room for its controls beside its text.
    Scene two = home({ withControls("media", { .before = button("prev"), .primary = button("play", true), .after = button("next") }),
                       withControls("timer", { .before = button("stop"), .primary = button("pause", true), .after = button("skip") }) });
    expect(two.size.x == t.homeCardWidth && two.tiles.size() == 2, "Home is homeCardWidth wide, two widgets in a row");
    const Tile& media = two.tiles[0];
    const Tile& timer = two.tiles[1];
    expect(std::abs(media.controls.width - column) < 0.01f &&
           std::abs(media.controls.x + media.controls.width - (media.frame.x + media.frame.width - t.tilePadding)) < 0.01f,
           "the column: small + large + small and gaps, at the widget's right inside its padding");
    const Placed* play = find(two, "tile/media/controls/primary/0/0");
    const Placed* pause = find(two, "tile/timer/controls/primary/0/0");
    expect(play && pause, "both primaries placed");
    expect(play && std::abs(centreX(*play) - (media.controls.x + media.controls.width / 2)) < 0.01f, "the primary in the column's middle");
    expect(play && pause && std::abs((centreX(*play) - media.frame.x) - (centreX(*pause) - timer.frame.x)) < 0.01f,
           "the primary on the same line in every widget");
    const Placed* title = find(two, "tile/media/0/0");
    expect(title && title->frame.x + title->frame.width <= media.controls.x - t.slotGap + 0.01f && title->frame.width >= t.tileTextMinWidth - 0.01f,
           "the text stops a gap before the column, and keeps tileTextMinWidth");

    // Clicks: a control is its own action; between controls nothing; the rest is the face.
    const Placed* prev = find(two, "tile/media/controls/before/0/0");
    auto hit = prev ? hitTest(two, { centreX(*prev), prev->frame.y + prev->frame.height / 2 }) : std::nullopt;
    expect(hit && hit->action.actionId == "prev" && hit->action.activityId == "media", "a click on a control sends its action for the widget");
    Vec2 between{ media.controls.x + 1, media.frame.y + 2 };
    expect(!tileAt(two, between), "the column isn't the face: a click in it, off the controls, opens nothing");
    expect(tileAt(two, { media.frame.x + 10, media.frame.y + 10 }) == &two.tiles[0], "the face opens the card");

    // Tab: the widget, then its controls left to right, then the next widget (the keys reach them, decision 163).
    std::vector<std::string> order;
    for (const Focusable& f : focusables(two))
        if (f.id.rfind("tab:", 0) != 0) order.push_back(f.id);
    std::vector<std::string> expected{ "tile:media", "el:tile/media/controls/before/0/0", "el:tile/media/controls/primary/0/0",
                                       "el:tile/media/controls/after/0/0", "tile:timer", "el:tile/timer/controls/before/0/0",
                                       "el:tile/timer/controls/primary/0/0", "el:tile/timer/controls/after/0/0" };
    expect(order == expected, "Tab: each widget, then its controls, then the next");
    auto onWidget = findFocusable(two, "tile:media");
    expect(onWidget && tileAt(two, onWidget->point) == &two.tiles[0], "Enter on a widget clicks its face, not a control");

    // One control alone keeps the column and sits in its middle, on the primary's line.
    Scene lone = home({ withControls("bt", { .primary = Switch{ .on = true, .actionId = "toggle" } }),
                        withControls("media", { .before = button("prev"), .primary = button("play", true), .after = button("next") }) });
    const Placed* knob = find(lone, "tile/bt/controls/primary/0/0");
    const Placed* lonePlay = find(lone, "tile/media/controls/primary/0/0");
    expect(knob && lonePlay && std::abs((centreX(*knob) - lone.tiles[0].frame.x) - (centreX(*lonePlay) - lone.tiles[1].frame.x)) < 0.01f,
           "a lone switch sits where the primaries do");

    // Too narrow: a one-column widget can't keep tileTextMinWidth beside the column, so it shows none.
    Scene narrow = home({ withControls("a", { .primary = button("go", true) }, false), withControls("b", {}, true),
                          withControls("c", { .primary = button("go", true) }, false) });
    expect(narrow.tiles.size() == 3 && narrow.tiles[0].controls.width == 0 && !find(narrow, "tile/a/controls/primary/0/0"),
           "too narrow for its text: no controls, the widget only informs (decision 019)");

    // The provider decides: no controls given, none shown.
    Scene none = home({ withControls("x", {}) });
    expect(none.tiles.size() == 1 && none.tiles[0].controls.width == 0 && none.tiles[0].face().width == none.tiles[0].frame.width,
           "no controls: the whole widget is its face");
}

} // namespace cup::test
