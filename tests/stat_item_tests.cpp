// The measurement items (decision 172): how big they are, where their parts
// go, which are buttons, and the Columns card they sit in.

#include "core/hit_test.h"
#include "core/item_metrics.h"
#include "core/keyboard.h"
#include "core/layout.h"
#include "core/stat_parts.h"
#include "core/tokens.h"
#include "expect.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cup::test {
namespace {

bool inside(Rect inner, Rect outer)
{
    return inner.x >= outer.x - 0.01f && inner.y >= outer.y - 0.01f && inner.x + inner.width <= outer.x + outer.width + 0.01f &&
           inner.y + inner.height <= outer.y + outer.height + 0.01f;
}

Scene shown(Content content)
{
    IslandState state;
    state = reduce(state, PanelPublished{ { .id = "card", .content = std::move(content), .providerId = "p" } }, Ms{ 0 });
    state = reduce(state, PillClicked{}, Ms{ 0 });
    return layout(state, Ms{ 0 });
}

const Placed* at(const Scene& scene, const std::string& key)
{
    for (const Placed& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

void graphTests()
{
    const Tokens& t = tokens();
    Rect frame{ 10, 20, 200, t.graphTallHeight };
    expect(graphHeight(GraphSize::Tall) == t.graphTallHeight && graphHeight(GraphSize::Low) == t.graphLowHeight &&
           graphHeight(GraphSize::Strip) == t.graphStripHeight, "three heights, from tokens");
    expect(heightOf(Graph{ .size = GraphSize::Low }, {}, 0) == t.graphLowHeight && !fixedWidthOf(Graph{}, 0, {}), "as tall as its size, as wide as it's given");

    Graph bare{ .samples = { 0, 0.5f, 1 } };
    GraphParts plain = graphParts(bare, frame);
    expect(plain.plot == frame && plain.texts.width == 0, "no tile: the plot is the item");
    Graph tile{ .tile = true, .valueLarge = true };
    GraphParts parts = graphParts(tile, frame);
    expect(parts.plot.y == frame.y + t.graphTallPlotTop && parts.plot.y + parts.plot.height == frame.y + frame.height && parts.plot.width == frame.width,
           "a tile: the plot under its texts, to its bottom and sides");
    expect(inside(parts.texts, frame) && parts.texts.x == frame.x + t.graphInset && parts.texts.y + parts.texts.height <= parts.plot.y + 0.01f,
           "its texts inset, over the plot");
    tile.size = GraphSize::Low;
    expect(graphParts(tile, { 0, 0, 100, t.graphLowHeight }).plot.y == t.graphLowPlotTop, "a low tile's plot starts higher");

    // The line: left to right across the plot, 1 at its top.
    auto runs = graphLines(bare, plain.plot);
    expect(runs.size() == 1 && runs[0].size() == 3 && runs[0][0] == Vec2{ frame.x, frame.y + frame.height } &&
           runs[0][2] == Vec2{ frame.x + frame.width, frame.y } && runs[0][1].x == frame.x + frame.width / 2, "samples span the plot");
    bare.capacity = 5;
    runs = graphLines(bare, plain.plot);
    expect(runs[0][0].x == frame.x + frame.width / 2 && runs[0][2].x == frame.x + frame.width, "a history just begun ends at the right edge");
    bare.capacity = 2; // less than there are: ignored
    expect(graphLines(bare, plain.plot)[0][0].x == frame.x, "a capacity under the count: the samples span it");
    Graph gappy{ .samples = { 0.2f, -1, -1, 0.4f, 0.6f, std::numeric_limits<float>::quiet_NaN(), 3, -0.5f } };
    runs = graphLines(gappy, plain.plot);
    expect(runs.size() == 3 && runs[0].size() == 1 && runs[1].size() == 2 && runs[2].size() == 1, "no reading breaks the line");
    expect(runs[2][0].y == frame.y && runs[1][0].x == frame.x + frame.width * 3 / 7, "over 1 is the top; a sample keeps its place in time");
    expect(graphLines(Graph{}, frame).empty() && graphLines(Graph{ .samples = { 0.5f } }, frame).empty(), "nothing, or one sample alone: no line");
    expect(!graphLimitY(bare, frame) && graphLimitY(Graph{ .limit = 0.75f }, frame) == frame.y + frame.height / 4, "the limit line's height");
}

void levelTests()
{
    const Tokens& t = tokens();
    Rect frame{ 5, 5, 100 + 3 * t.levelsGap, t.levelsHeight };
    Levels levels{ { { 0.5f }, { 1.5f, true }, { -1 }, { std::numeric_limits<float>::quiet_NaN() } } };
    expect(heightOf(levels, {}, 0) == t.levelsHeight && !fixedWidthOf(levels, 0, {}), "a strip: levelsHeight tall, stretching");
    auto bars = levelFrames(levels, frame);
    expect(bars.size() == 4 && bars[0].bar.width == 25 && bars[1].bar.x == frame.x + 25 + t.levelsGap &&
           std::abs(bars[3].bar.x + bars[3].bar.width - (frame.x + frame.width)) < 0.01f, "equal bars, a gap between, across the frame");
    expect(bars[0].fill.height == frame.height / 2 && bars[0].fill.y + bars[0].fill.height == frame.y + frame.height, "filled from the bottom");
    expect(bars[1].fill.height == frame.height && bars[2].fill.height == 0 && bars[3].fill.height == 0, "clamped; garbage is empty");
    expect(levelAt(levels, frame, { frame.x + 1, frame.y + 1 }) == 0u && levelAt(levels, frame, { frame.x + frame.width - 1, frame.y + 1 }) == 3u &&
           !levelAt(levels, frame, { frame.x - 1, frame.y }) && !levelAt(Levels{}, frame, { frame.x + 1, frame.y + 1 }), "the bar under the pointer");
    Levels many;
    many.bars.resize(64);
    auto thin = levelFrames(many, { 0, 0, 64, 10 });
    expect(thin.size() == 64 && thin[1].bar.x == thin[0].bar.x + thin[0].bar.width && thin[0].bar.width == 1, "too many for gaps: bars only");
    expect(levelFrames(Levels{}, frame).empty(), "none");
}

void batteryTests()
{
    const Tokens& t = tokens();
    float shape = t.batterySize.x + t.batteryStroke + t.batteryCapWidth;
    Battery plain{ .level = 0.5f };
    expect(fixedWidthOf(plain, 0, {}) == shape && heightOf(plain, {}, 0) == t.batterySize.y, "alone: its shape's size");
    Rect frame{ 10, 10, shape, t.batterySize.y };
    BatteryParts parts = batteryParts(plain, frame, {});
    expect(parts.body == Rect{ 10, 10, t.batterySize.x, t.batterySize.y } && parts.cap.x > parts.body.x + parts.body.width &&
           inside(parts.cap, frame) && parts.label.width == 0 && parts.value.width == 0, "the body, then its nub");
    float inner = t.batterySize.x - 4 * t.batteryStroke;
    expect(inside(parts.fill, parts.body) && std::abs(parts.fill.width - inner / 2) < 0.01f && parts.fill.x == parts.body.x + 2 * t.batteryStroke,
           "the fill inside the outline, from the left");
    expect(batteryParts({ .level = 7 }, frame, {}).fill.width == inner && batteryParts({ .level = -1 }, frame, {}).fill.width == 0, "clamped");

    Battery labelled{ .level = 0.8f, .label = "Left", .value = "82%" };
    float width = *fixedWidthOf(labelled, 0, {});
    expect(width > shape + 2 * t.batteryTextGap && heightOf(labelled, {}, 0) == lineHeight(TextStyle::Caption), "with texts: wider, a caption's line tall");
    Rect wide{ 0, 0, width, lineHeight(TextStyle::Caption) };
    BatteryParts all = batteryParts(labelled, wide, {});
    expect(all.label.x == 0 && all.label.width > 0 && all.body.x == all.label.width + t.batteryTextGap &&
           all.value.x == all.cap.x + all.cap.width + t.batteryTextGap && std::abs(all.value.x + all.value.width - width) < 0.01f,
           "the label, the battery, the value, in a row");
    expect(std::abs(all.body.y + all.body.height / 2 - wide.height / 2) < 0.01f, "the battery centred on the texts' line");
}

void tileTests()
{
    const Tokens& t = tokens();
    Rect frame{ 20, 30, 142, t.graphLowHeight };
    LevelTile disk{ .glyph = Glyph::Drive, .caption = "Disk C:", .value = "71%", .level = 0.71f, .detail = "148 GB free", .actionId = "open" };
    expect(heightOf(disk, {}, 0) == t.graphLowHeight && !fixedWidthOf(disk, 0, {}) && heightOf(BatteryTile{}, {}, 0) == t.graphLowHeight,
           "tiles: a low graph's height, the width they're given");
    LevelTileParts parts = levelTileParts(disk, frame, {});
    for (Rect r : { parts.glyph, parts.caption, parts.value, parts.bar, parts.detail }) expect(inside(r, frame), "a level tile's parts inside it");
    expect(parts.glyph.x == frame.x + t.graphInset && parts.caption.x > parts.glyph.x + parts.glyph.width - 0.01f &&
           parts.caption.x + parts.caption.width <= parts.value.x && std::abs(parts.value.x + parts.value.width - (frame.x + frame.width - t.graphInset)) < 0.01f,
           "glyph, caption, and the value at the right");
    expect(parts.bar.y > parts.caption.y + parts.caption.height && parts.detail.y > parts.bar.y + parts.bar.height &&
           std::abs(parts.fill - parts.bar.width * 0.71f) < 0.01f && parts.bar.height == t.progressHeight, "the bar under them, the detail under it");
    expect(std::abs((parts.caption.y - frame.y) - (frame.y + frame.height - parts.detail.y - parts.detail.height)) < 0.01f, "centred in the tile");

    BatteryTile devices{ .cells = { { Glyph::Headphones, 0.82f, false, "82%" }, { Glyph::Mouse, 0.1f, true, "10%" }, { Glyph::Keyboard, 0.45f, false, "45%" } } };
    auto cells = batteryTileParts(devices, frame);
    expect(cells.size() == 3, "a cell per device");
    for (std::size_t i = 0; i < cells.size(); ++i) {
        float middle = frame.x + frame.width * (static_cast<float>(i) + 0.5f) / 3;
        expect(inside(cells[i].glyph, frame) && inside(cells[i].battery.body, frame) && inside(cells[i].value, frame) &&
               std::abs(cells[i].glyph.x + cells[i].glyph.width / 2 - middle) < 0.01f && cells[i].battery.body.y > cells[i].glyph.y + cells[i].glyph.height &&
               cells[i].value.y > cells[i].battery.body.y + cells[i].battery.body.height, "glyph over battery over percent, in its column");
    }
    expect(cells[1].battery.fill.width < cells[0].battery.fill.width && batteryTileParts(BatteryTile{}, frame).empty(), "each its own level; none: no cells");
}

void progressTests()
{
    const Tokens& t = tokens();
    Progress level{ .level = 0.4f };
    Progress playing{ .timeline = { .position = Ms{ 500 }, .duration = Ms{ 1000 } } };
    expect(fractionOf(level, Ms{ 0 }) == 0.4f && fractionOf(playing, Ms{ 0 }) == 0.5f && fractionOf({ .level = 3.0f }, Ms{ 0 }) == 1 &&
           fractionOf({ .timeline = playing.timeline, .level = 0.1f }, Ms{ 0 }) == 0.1f, "a level wins over the timeline, clamped");
    expect(heightOf(level, {}, 0) == t.progressHeight && heightOf(Progress{ .level = 0.1f, .thick = true }, {}, 0) == t.thickProgressHeight, "a thick bar");
    expect(!fixedWidthOf(level, 0, {}) && fixedWidthOf(Progress{ .level = 0.1f, .stub = true }, 0, {}) == t.stubBarWidth, "a stub: a fixed, short bar");
}

// Tiles are buttons as a whole; a grid's gaps are one size.
void buttonTests()
{
    const Tokens& t = tokens();
    Content c{ .label = "Tiles", .card = CardLayout::Centred };
    Row top{ .items = { Graph{ .tile = true, .actionId = "a" }, Graph{ .tile = true }, Graph{ .tile = true, .actionId = "c" } }, .even = true };
    Row bottom{ .items = { Graph{ .size = GraphSize::Low, .tile = true, .actionId = "d" }, LevelTile{ .actionId = "e" }, BatteryTile{ .actionId = "f" } },
                .even = true, .tileGap = true };
    c.expandedCenter = { top, bottom };
    Scene scene = shown(c);
    expect(scene.size.y == t.expandedSize.y, "two rows of tiles fit the height every card has");
    const Placed* a = at(scene, "expandedCenter/0/0");
    const Placed* b = at(scene, "expandedCenter/0/1");
    const Placed* d = at(scene, "expandedCenter/1/0");
    const Placed* e = at(scene, "expandedCenter/1/1");
    const Placed* f = at(scene, "expandedCenter/1/2");
    expect(a && b && d && e && f && a->frame.height == t.graphTallHeight && d->frame.height == t.graphLowHeight && e->frame.height == d->frame.height &&
           f->frame.y == d->frame.y, "each at its height, a row's tiles level");
    expect(std::abs((b->frame.x - a->frame.x - a->frame.width) - (d->frame.y - a->frame.y - a->frame.height)) < 0.01f &&
           std::abs(b->frame.x - a->frame.x - a->frame.width - t.itemSpacing) < 0.01f, "as far under as beside: itemSpacing");
    expect(a->frame.width == b->frame.width && a->cornerRadius == t.groupRadius && e->cornerRadius == t.groupRadius, "equal columns, a group's corners");
    auto centre = [](const Placed& p) { return Vec2{ p.frame.x + p.frame.width / 2, p.frame.y + p.frame.height / 2 }; };
    auto hitA = hitTest(scene, centre(*a));
    expect(hitA && hitA->action.actionId == "a" && hitA->action.providerId == "p" && hitA->action.activityId == "card" && hitA->key == a->key,
           "a click anywhere on a tile is its action");
    expect(!hitTest(scene, centre(*b)), "a tile with no action isn't a button");
    expect(hitTest(scene, centre(*e))->action.actionId == "e" && hitTest(scene, centre(*f))->action.actionId == "f" &&
           hitTest(scene, { d->frame.x + 2, d->frame.y + d->frame.height - 2 })->action.actionId == "d", "the other kinds of tile too, to their corners");
    expect(lightsAt(*a, centre(*a)), "lit under the pointer");
    auto focus = focusables(scene);
    auto focusable = [&](const Placed* p) { return std::any_of(focus.begin(), focus.end(), [&](const Focusable& x) { return x.id == "el:" + p->key; }); };
    expect(focusable(a) && !focusable(b) && focusable(e) && focusable(f), "the keys reach the tiles that are buttons");
    expect(tileAction(Text{ "x" }).empty() && tileAction(Graph{ .actionId = "g" }) == "g", "tileAction: only the tiles");
}

// CardLayout::Columns: a header across, a block at the left, a column at the right.
void columnTests()
{
    const Tokens& t = tokens();
    Content c{ .label = "Columns", .card = CardLayout::Columns, .wideCard = true };
    c.expandedCenter = { row({ Text{ "Header", TextStyle::Title } }) };
    c.expandedLeading = { row({ Graph{ .size = GraphSize::Strip } }), row({ Levels{ { { 0.5f } } } }) };
    c.expandedTrailing = { row({ Text{ "one" } }), row({ Text{ "two" } }) };
    Scene scene = shown(c);
    float p = t.expandedPadding;
    const Placed* header = at(scene, "expandedCenter/0/0");
    const Placed* graph = at(scene, "expandedLeading/0/0");
    const Placed* strip = at(scene, "expandedLeading/1/0");
    const Placed* one = at(scene, "expandedTrailing/0/0");
    const Placed* two = at(scene, "expandedTrailing/1/0");
    expect(scene.size.x == t.wideCardWidth && scene.size.y == t.expandedSize.y, "a wide card, the usual height when its columns fit");
    expect(header && graph && strip && one && two && header->frame.y == p && header->frame.x == p, "the header at the top");
    float top = p + header->frame.height + t.slotGap;
    expect(graph->frame.x == p && std::abs(graph->frame.width - (scene.size.x - 2 * p - t.sideColumnWidth - t.columnGap)) < 0.01f &&
           strip->frame.width == graph->frame.width, "the left block: what the side column leaves");
    expect(std::abs(one->frame.x - (scene.size.x - p - t.sideColumnWidth)) < 0.01f && one->frame.width == t.sideColumnWidth &&
           std::abs(one->frame.x - (graph->frame.x + graph->frame.width + t.columnGap)) < 0.01f, "the side column, columnGap from it");
    for (const Placed* x : { graph, strip, one, two })
        expect(x->frame.y >= top - 0.01f && x->frame.y + x->frame.height <= scene.size.y - p + 0.01f, "both under the header, inside the card");
    float room = scene.size.y - p - top;
    expect(std::abs((graph->frame.y - top) - (top + room - strip->frame.y - strip->frame.height)) < 0.01f &&
           std::abs((one->frame.y - top) - (top + room - two->frame.y - two->frame.height)) < 0.01f, "each column's rows centred in it");

    // Not a page: the card grows to its taller column.
    for (int i = 0; i < 12; ++i) c.expandedTrailing.push_back(row({ Text{ "more" } }));
    Scene tall = shown(c);
    const Placed* last = at(tall, "expandedTrailing/13/0");
    expect(tall.size.y > t.expandedSize.y && last && std::abs(last->frame.y + last->frame.height - (tall.size.y - p)) < 0.51f, "it grows for its taller column");
    // A page: the height of the rows it's sized like, whatever its own.
    c.sizedLike = { row({ Text{ "main" } }) };
    expect(shown(c).size.y == t.expandedSize.y, "a page keeps its main view's height (Content::sizedLike)");
    // A narrow card: the side column takes half at most.
    c.wideCard = false;
    c.expandedTrailing.resize(1);
    Scene narrow = shown(c);
    expect(at(narrow, "expandedTrailing/0/0")->frame.width <= (narrow.size.x - 2 * p) / 2 + 0.01f && at(narrow, "expandedLeading/0/0")->frame.width > 0,
           "never more than half the card");
}

} // namespace

void runStatItemTests()
{
    graphTests();
    levelTests();
    batteryTests();
    tileTests();
    progressTests();
    buttonTests();
    columnTests();
}

} // namespace cup::test
