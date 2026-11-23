// The measurement tiles (decision 174): how big they are, where their parts
// go, that they are nothing to click, and the Columns card they sit in.

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

void statTileTests()
{
    const Tokens& t = tokens();
    StatTile big{ .glyph = Glyph::Chip, .caption = "CPU", .detail = "46°", .value = "14%", .level = 0.14f, .large = true };
    StatTile mini{ .glyph = Glyph::Drive, .caption = "Disk", .value = "71%", .level = 0.71f };
    StatTile line{ .glyph = Glyph::Network, .caption = "Network", .value = "↓ 9.9 ↑ 0.3 MB/s" };
    expect(heightOf(big, {}, 0) == t.statTileHeight && heightOf(mini, {}, 0) == t.statMiniHeight && heightOf(line, {}, 0) == t.statMiniHeight &&
           !fixedWidthOf(big, 0, {}) && !fixedWidthOf(mini, 0, {}), "a large tile and a small one, from tokens; as wide as they're given");

    Rect frame{ 10, 20, 142, t.statTileHeight };
    StatTileParts parts = statTileParts(big, frame, {});
    for (Rect r : { parts.glyph, parts.caption, parts.aside, parts.value, parts.bar }) expect(inside(r, frame), "a large tile's parts inside it");
    expect(parts.glyph.x == frame.x + t.tileInset && parts.glyph.y == frame.y + t.tileInset && parts.caption.x > parts.glyph.x + parts.glyph.width &&
           parts.caption.width > 0 && parts.caption.x + parts.caption.width <= parts.aside.x &&
           std::abs(parts.aside.x + parts.aside.width - (frame.x + frame.width - t.tileInset)) < 0.01f && parts.aside.y == parts.glyph.y,
           "its glyph and caption at the top left, its detail at the top right");
    expect(std::abs(parts.bar.y + parts.bar.height - (frame.y + frame.height - t.tileInset)) < 0.01f && parts.bar.height == t.progressHeight &&
           parts.bar.width == frame.width - 2 * t.tileInset && std::abs(parts.fill - parts.bar.width * 0.14f) < 0.01f, "its level along the bottom");
    expect(parts.value.height == t.statValueLineHeight && parts.value.y + parts.value.height <= parts.bar.y &&
           parts.value.y >= parts.glyph.y + parts.glyph.height && parts.value.x == parts.glyph.x, "its reading between them, over the bar");

    Rect small{ 0, 0, 218, t.statMiniHeight };
    StatTileParts disk = statTileParts(mini, small, {});
    for (Rect r : { disk.glyph, disk.caption, disk.aside, disk.bar }) expect(inside(r, small), "a small tile's parts inside it");
    expect(disk.value.width == 0 && disk.bar.y > disk.glyph.y + disk.glyph.height && std::abs(disk.fill - disk.bar.width * 0.71f) < 0.01f &&
           std::abs(disk.glyph.y - (small.height - (disk.bar.y + disk.bar.height))) < 0.01f, "one line over its bar, centred together; no big reading");
    StatTileParts net = statTileParts(line, small, {});
    expect(net.bar.width == 0 && net.bar.height == 0 && net.fill == 0 && std::abs(net.glyph.y + net.glyph.height / 2 - small.height / 2) < 0.01f,
           "no level: one line, centred, no bar");
    expect(net.aside.width > 0 && inside(net.aside, small) && (net.caption.width == 0 || net.caption.x + net.caption.width <= net.aside.x),
           "what's at the right keeps its width");
    // The caption gives way whole when there's no room for it beside what's at the right.
    StatTileParts tight = statTileParts(line, { 0, 0, 150, t.statMiniHeight }, {});
    StatTileParts roomy = statTileParts(line, { 0, 0, 400, t.statMiniHeight }, {});
    expect(tight.caption.width == 0 && tight.aside.width > 0 && roomy.caption.width > 0, "a caption that doesn't fit is left out, not cut");
    StatTile over{ .value = "x", .level = 7.0f, .large = true };
    StatTile junk{ .value = "x", .level = std::numeric_limits<float>::quiet_NaN() };
    expect(statTileParts(over, frame, {}).fill == frame.width - 2 * t.tileInset && statTileParts(junk, small, {}).fill == 0, "a level clamped; garbage empty");
}

void batteryTests()
{
    const Tokens& t = tokens();
    float shape = t.batterySize.x + t.batteryStroke + t.batteryCapWidth;
    expect(batteryShapeWidth() == shape, "a battery: its body, a gap and its nub");
    BatteryParts parts = batteryShape(0.5f, 10, 30);
    expect(parts.body == Rect{ 10, 30 - t.batterySize.y / 2, t.batterySize.x, t.batterySize.y } && parts.cap.x > parts.body.x + parts.body.width &&
           std::abs(parts.cap.x + parts.cap.width - (10 + shape)) < 0.01f && std::abs(parts.cap.y + parts.cap.height / 2 - 30) < 0.01f,
           "the body centred on its line, then its nub");
    float inner = t.batterySize.x - 4 * t.batteryStroke;
    expect(inside(parts.fill, parts.body) && std::abs(parts.fill.width - inner / 2) < 0.01f && parts.fill.x == parts.body.x + 2 * t.batteryStroke,
           "the charge inside the outline, from the left");
    expect(batteryShape(7, 0, 0).fill.width == inner && batteryShape(-1, 0, 0).fill.width == 0, "clamped");
    expect(parts.bodyRadius > 0 && parts.bodyRadius <= t.batterySize.y / 2 && parts.fillRadius >= 0 && parts.fillRadius < parts.bodyRadius,
           "its corners: the charge's nested in the outline's");
}

void batteryTileTests()
{
    const Tokens& t = tokens();
    expect(heightOf(BatteryTile{}, {}, 0) == batteryTileHeight() && batteryTileHeight() == 2 * t.statMiniHeight + t.itemSpacing &&
           !fixedWidthOf(BatteryTile{}, 0, {}), "as tall as two small tiles and their gap");
    expect(batteryTileLines() == 3, "three devices' rows fit it");
    Rect frame{ 5, 7, 218, batteryTileHeight() };
    BatteryTile three{ .rows = { { Glyph::Headphones, "WH-1000XM5", 0.82f, false, "82%" }, { Glyph::Mouse, "MX Master 3S", 0.1f, true, "10%" },
                                 { Glyph::Keyboard, "Keychron K3", 0.45f, false, "45%" } } };
    BatteryTileParts parts = batteryTileParts(three, frame, {});
    expect(parts.rows.size() == 3 && !parts.alone && parts.note.width == 0, "a row per device");
    for (std::size_t i = 0; i < parts.rows.size(); ++i) {
        const BatteryRowParts& row = parts.rows[i];
        expect(inside(row.glyph, frame) && inside(row.name, frame) && inside(row.battery.body, frame) && inside(row.battery.cap, frame) &&
               inside(row.value, frame) && row.bubble.width == 0, "a row's parts inside the tile");
        expect(row.glyph.x == frame.x + t.tileInset && row.name.x > row.glyph.x + row.glyph.width && row.name.x + row.name.width <= row.battery.body.x &&
               row.battery.cap.x + row.battery.cap.width <= row.value.x && std::abs(row.value.x + row.value.width - (frame.x + frame.width - t.tileInset)) < 0.01f,
               "glyph, name, battery, percent, left to right");
        if (i > 0) expect(std::abs(row.name.y - parts.rows[i - 1].name.y - t.batteryRowHeight) < 0.01f && row.value.x == parts.rows[0].value.x,
                          "rows batteryRowHeight apart, the percents in one column");
    }
    expect(std::abs((parts.rows[0].name.y - frame.y) - (frame.y + frame.height - parts.rows[2].name.y - parts.rows[2].name.height)) < 0.01f,
           "the rows centred in the tile");
    expect(parts.rows[1].battery.fill.width < parts.rows[0].battery.fill.width, "each its own charge");

    // A note under the rows takes a row's place.
    BatteryTile more{ .rows = { three.rows[0], three.rows[1] }, .note = "+3 more" };
    BatteryTileParts noted = batteryTileParts(more, frame, {});
    expect(noted.rows.size() == 2 && !noted.alone && inside(noted.note, frame) && std::abs(noted.note.y - noted.rows[1].name.y - t.batteryRowHeight) < 0.01f &&
           noted.note.height == t.batteryRowHeight, "a note in the next row's place");
    // No devices: the note alone, across the tile.
    BatteryTileParts none = batteryTileParts(BatteryTile{ .note = "No devices connected" }, frame, {});
    expect(none.rows.empty() && none.note == frame && batteryTileParts(BatteryTile{}, frame, {}).note.width == 0, "none: the note is the tile");
    // One device alone: its glyph in a circle, its name over its battery and percent.
    BatteryTile one{ .rows = { { Glyph::Earbuds, "Galaxy Buds2 Pro", 0.74f, false, "74%" } } };
    BatteryTileParts alone = batteryTileParts(one, frame, {});
    const BatteryRowParts& row = alone.rows[0];
    expect(alone.alone && alone.rows.size() == 1 && row.bubble.width == t.deviceBubbleSize && row.bubble.height == t.deviceBubbleSize && inside(row.bubble, frame) &&
           std::abs(row.bubble.y + row.bubble.height / 2 - (frame.y + frame.height / 2)) < 0.01f && inside(row.glyph, row.bubble) &&
           row.glyph.width == t.largeIconSize, "one device: a circle with its glyph, big");
    expect(row.name.x > row.bubble.x + row.bubble.width && inside(row.name, frame) && row.battery.body.x == row.name.x &&
           row.battery.body.y > row.name.y + row.name.height - 0.01f && row.value.x > row.battery.cap.x + row.battery.cap.width && inside(row.value, frame) &&
           row.value.height == lineHeight(TextStyle::CardTitle) && row.name.height == lineHeight(TextStyle::Body), "its name over its battery and percent");
    BatteryTile oneNoted{ .rows = one.rows, .note = "+1 more" };
    expect(!batteryTileParts(oneNoted, frame, {}).alone, "one row and a note: rows, not the large treatment");
}

// Tiles are nothing to click; a grid's gaps are one size; a Stat text has its size.
void gridTests()
{
    const Tokens& t = tokens();
    expect(fontSize(TextStyle::Stat) == t.statValueFontSize, "TextStyle::Stat: its own size");
    Content c{ .label = "Tiles", .card = CardLayout::Columns };
    c.expandedCenter = { Row{ .items = { StatTile{ .large = true }, StatTile{ .large = true }, StatTile{ .large = true } }, .even = true } };
    Row under = row({ StatTile{} });
    under.tileGap = true;
    c.expandedLeading = { row({ StatTile{} }), under };
    c.expandedTrailing = { row({ BatteryTile{} }) };
    Scene scene = shown(c);
    float p = t.expandedPadding;
    expect(scene.size.y == t.expandedSize.y && scene.size.x == t.expandedSize.x, "the grid fills the card every page has, exactly");
    const Placed* a = at(scene, "expandedCenter/0/0");
    const Placed* b = at(scene, "expandedCenter/0/1");
    const Placed* c3 = at(scene, "expandedCenter/0/2");
    const Placed* disk = at(scene, "expandedLeading/0/0");
    const Placed* net = at(scene, "expandedLeading/1/0");
    const Placed* bt = at(scene, "expandedTrailing/0/0");
    expect(a && b && c3 && disk && net && bt, "every tile is placed");
    if (!a || !b || !c3 || !disk || !net || !bt) return;
    expect(a->frame.y == p && a->frame.x == p && a->frame.height == t.statTileHeight && a->frame.width == b->frame.width &&
           std::abs(c3->frame.x + c3->frame.width - (scene.size.x - p)) < 0.01f && std::abs(b->frame.x - a->frame.x - a->frame.width - t.itemSpacing) < 0.01f,
           "the top row: equal tiles across the card, itemSpacing apart");
    expect(std::abs(disk->frame.y - (a->frame.y + a->frame.height + t.columnGap)) < 0.01f && disk->frame.x == p && disk->frame.height == t.statMiniHeight &&
           std::abs(net->frame.y - disk->frame.y - disk->frame.height - t.itemSpacing) < 0.01f &&
           std::abs(net->frame.y + net->frame.height - (scene.size.y - p)) < 0.01f, "the left column: one small tile over another, to the card's padding");
    expect(std::abs(bt->frame.y - disk->frame.y) < 0.01f && std::abs(bt->frame.y + bt->frame.height - (net->frame.y + net->frame.height)) < 0.01f &&
           std::abs(bt->frame.width - disk->frame.width) < 0.01f && std::abs(bt->frame.x - (disk->frame.x + disk->frame.width + t.columnGap)) < 0.01f &&
           std::abs(bt->frame.x + bt->frame.width - (scene.size.x - p)) < 0.01f, "the right column: as tall as both, half the card, columnGap away");
    expect(t.columnGap == t.itemSpacing, "one gap in the grid, across and down (tokens)");
    for (const Placed* tile : { a, disk, bt }) {
        expect(tile->cornerRadius == t.groupRadius, "a group's corners");
        expect(!hitTest(scene, { tile->frame.x + tile->frame.width / 2, tile->frame.y + tile->frame.height / 2 }), "a click on a tile is no action");
    }
    auto focus = focusables(scene);
    expect(std::none_of(focus.begin(), focus.end(), [](const Focusable& f) { return f.id.rfind("el:", 0) == 0; }), "the keys have nothing to land on");
}

// CardLayout::Columns: rows across the top, two equal columns under them.
void columnTests()
{
    const Tokens& t = tokens();
    Content c{ .label = "Columns", .card = CardLayout::Columns };
    c.expandedCenter = { row({ Text{ "Header", TextStyle::Title } }) };
    c.expandedLeading = { row({ Text{ "left" } }) };
    c.expandedTrailing = { row({ Text{ "one" } }), row({ Text{ "two" } }) };
    Scene scene = shown(c);
    float p = t.expandedPadding;
    const Placed* header = at(scene, "expandedCenter/0/0");
    const Placed* left = at(scene, "expandedLeading/0/0");
    const Placed* one = at(scene, "expandedTrailing/0/0");
    const Placed* two = at(scene, "expandedTrailing/1/0");
    expect(scene.size.y == t.expandedSize.y && header && left && one && two && header->frame.y == p && header->frame.x == p, "the top rows at the top");
    if (!header || !left || !one || !two) return;
    float top = p + header->frame.height + t.columnGap, half = (scene.size.x - 2 * p - t.columnGap) / 2;
    expect(left->frame.x == p && std::abs(left->frame.width - half) < 0.01f && std::abs(one->frame.width - half) < 0.01f &&
           std::abs(one->frame.x - (p + half + t.columnGap)) < 0.01f, "two equal columns, columnGap apart");
    float room = scene.size.y - p - top;
    expect(std::abs((left->frame.y - top) - (top + room - left->frame.y - left->frame.height)) < 0.01f &&
           std::abs((one->frame.y - top) - (top + room - two->frame.y - two->frame.height)) < 0.01f && one->frame.y >= top - 0.01f,
           "each column's rows centred in it, under the top rows");
    // The card grows to its taller column; a page keeps the height it's sized like.
    for (int i = 0; i < 14; ++i) c.expandedTrailing.push_back(row({ Text{ "more" } }));
    Scene tall = shown(c);
    const Placed* last = at(tall, "expandedTrailing/15/0");
    expect(tall.size.y > t.expandedSize.y && last && std::abs(last->frame.y + last->frame.height - (tall.size.y - p)) < 0.51f, "it grows for its taller column");
    c.sizedLike = { row({ Text{ "main" } }) };
    expect(shown(c).size.y == t.expandedSize.y, "a page keeps its main view's height (Content::sizedLike)");
}

} // namespace

void runStatItemTests()
{
    statTileTests();
    batteryTests();
    batteryTileTests();
    gridTests();
    columnTests();
}

} // namespace cup::test
