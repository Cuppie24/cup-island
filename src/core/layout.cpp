#include "core/layout.h"

#include "core/dialog.h"
#include "core/home.h"
#include "core/slot_layout.h"
#include "core/split.h"
#include "core/tabs.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

Vec2 sizeFor(Mode mode)
{
    const Tokens& t = tokens();
    switch (mode) {
    case Mode::Idle: return t.idleSize;
    case Mode::Compact: return t.compactSize;
    case Mode::Split: return t.compactSize;
    case Mode::Alert: return t.alertSize;
    case Mode::Expanded: return t.expandedSize;
    case Mode::Receptive: return t.receptiveSize;
    case Mode::Hidden: return t.hiddenSize;
    }
    return t.idleSize;
}

Source sourceFor(const IslandState& s, Mode mode)
{
    if (mode == Mode::Alert) return { &s.alert->content, s.alert->providerId, s.alert->activityId };
    if (mode != Mode::Compact && mode != Mode::Split && mode != Mode::Expanded) return {};
    if (mode != Mode::Expanded) return { &s.activities[0].content, s.activities[0].providerId, s.activities[0].id }; // the top one
    // Expanded shows the card that was opened: an activity's, else a dormant panel's (decision 009). Home has none.
    const Draft* draft = s.draft ? &*s.draft : nullptr; // fills its TextInput (decision 016)
    if (const Activity* a = findActivity(s, s.expandedId)) return { &a->content, a->providerId, a->id, draft };
    if (const Panel* p = findPanel(s, s.expandedId)) return { &p->content, p->providerId, p->id, draft };
    return {};
}

// A Centred card grows to fit its rows (a to-do list), never below Expanded's
// size and never past the canvas (decision 016).
float centredCardHeight(const Content& c, Vec2 size, float tabSpace)
{
    const Tokens& t = tokens();
    float p = t.expandedPadding;
    float needed = slotHeight(c.expandedCenter, { p, p, size.x - 2 * p, size.y }) + 2 * p;
    return std::clamp(needed, t.expandedSize.y, std::max(t.expandedSize.y, t.canvas.y - tabSpace));
}

// The padding a mode keeps on every side of its content.
float paddingFor(Mode mode)
{
    const Tokens& t = tokens();
    switch (mode) {
    case Mode::Compact:
    case Mode::Split: return t.compactInset;
    case Mode::Alert: return t.alertPadding;
    case Mode::Expanded: return t.expandedPadding;
    default: return 0;
    }
}

// Where each slot sits inside the pill (PS §5.1). Every slot keeps the mode's
// padding from the pill's edges: with the nested corner radius that keeps
// rounded content concentric with the pill's corners.
std::vector<SlotFrame> slotFrames(Mode mode, Vec2 size, const Content& c)
{
    const Tokens& t = tokens();
    float p = paddingFor(mode);
    switch (mode) {
    case Mode::Compact:
    case Mode::Split: {
        float side = size.y - 2 * p, centerX = p + side + t.itemSpacing;
        return { { "compactLeading", &c.compactLeading, { p, p, side, side }, true },
                 { "compactCenter", &c.compactCenter, { centerX, p, std::max(0.0f, size.x - 2 * centerX), side } },
                 { "compactTrailing", &c.compactTrailing, { size.x - p - side, p, side, side }, true } };
    }
    case Mode::Alert: {
        float side = size.y - 2 * p, centerX = p + side + t.slotGap;
        return { { "alertLeading", &c.alertLeading, { p, p, side, side } },
                 { "alertCenter", &c.alertCenter, { centerX, p, size.x - centerX - p, side } } };
    }
    case Mode::Expanded: {
        if (c.card == CardLayout::Centred) // one stack across the whole card (decision 014)
            return { { "expandedCenter", &c.expandedCenter, { p, p, size.x - 2 * p, size.y - 2 * p } } };
        float art = t.expandedArtSize, y = p, centerX = p + art + t.slotGap, bottomY = y + art + t.slotGap;
        float corner = t.buttonSize, cornerX = size.x - p - corner;
        float centerRight = c.expandedTrailing.empty() ? size.x - p : cornerX - t.slotGap;
        return { { "expandedLeading", &c.expandedLeading, { p, y, art, art } },
                 { "expandedCenter", &c.expandedCenter, { centerX, y, centerRight - centerX, art } },
                 { "expandedTrailing", &c.expandedTrailing, { cornerX, y, corner, corner } },
                 { "expandedBottom", &c.expandedBottom, { p, bottomY, size.x - 2 * p, size.y - bottomY - p } } };
    }
    default:
        return {};
    }
}

// Split: the 2nd activity's `minimal` slot, else its compact leading one
// (PS §5.1). Same shape as the pill (straight top, rounded bottom), so its
// content gets the same nested corner radius.
BubbleScene bubbleFor(const IslandState& s, Ms now)
{
    const Activity& a = s.activities[1];
    BubbleScene bubble;
    bubble.frame = bubbleHomeFrame();
    bubble.activityId = a.id;
    if (s.bubbleDrag) {
        Rect f = bubble.frame;
        bubble.dragCentre = Vec2{ f.x + f.width / 2 + s.bubbleDrag->x, f.y + f.height / 2 + s.bubbleDrag->y };
    }

    float inset = tokens().compactInset, side = bubble.frame.width - 2 * inset;
    float nestedRadius = std::max(0.0f, pillCornerRadius({ bubble.frame.width, bubble.frame.height }) - inset);
    const Slot& slot = a.content.minimal.empty() ? a.content.compactLeading : a.content.minimal;
    placeSlot({ "minimal", &slot, { inset, inset, side, side }, true }, { &a.content, a.providerId, a.id }, nestedRadius, now,
              bubble.elements);
    return bubble;
}



// Idle: the local time, centred in the pill, once the platform has synced the clock.
void placeClock(const IslandState& s, Scene& scene, const MeasureText& measure)
{
    auto time = timeOfDay(s, scene.now);
    if (!time) return;
    Text text{ formatClock(*time, s.clock->hour24), TextStyle::Body };
    float width = std::min(measure(text) + 1, scene.size.x); // +1: rounding never trips the "…"
    Rect frame{ (scene.size.x - width) / 2, 0, width, scene.size.y };
    scene.elements.push_back({ .key = "clock", .item = text, .frame = frame, .hitFrame = frame });
    scene.clockTick = nextMinute(*time, scene.now);
}

} // namespace

Scene layout(const IslandState& state, Ms now, const MeasureText& measure)
{
    Scene scene;
    scene.now = now;
    scene.mode = mode(state);
    float tabSpace = tabStripSpace(state);
    scene.size = sizeFor(scene.mode);
    Source source = sourceFor(state, scene.mode);
    if (scene.mode == Mode::Expanded && source.content && source.content->card == CardLayout::Centred)
        scene.size.y = centredCardHeight(*source.content, scene.size, tabSpace);
    if (scene.mode == Mode::Expanded && state.homeOpen) scene.size.y = homeCardHeight(state, tokens().canvas.y - tabSpace);
    scene.size.y += tabSpace;
    scene.offset = { (tokens().canvas.x - scene.size.x) / 2, 0 };
    // On the top edge, inside the pill's right end: the bubble shrinks into it
    // and buds back out of it hanging from the edge, like the pill, never
    // floating up from the middle of a tall card.
    float tuckInset = std::min(scene.size.x, tokens().bubbleSize) / 2;
    scene.bubbleTuck = { scene.offset.x + scene.size.x - tuckInset, scene.offset.y };
    if (scene.mode == Mode::Split) scene.bubble = bubbleFor(state, now);
    scene.dropTarget = swapPreview(state);

    MeasureText measureText = measure ? measure : MeasureText{ estimateTextWidth };
    if (scene.mode == Mode::Idle) placeClock(state, scene, measureText);

    // The card's content keeps Expanded's size; the tab strip, if any, adds room below it (decision 009).
    Vec2 contentSize{ scene.size.x, scene.size.y - tabSpace };
    float nestedRadius = std::max(0.0f, pillCornerRadius(scene.size) - paddingFor(scene.mode));
    if (scene.mode == Mode::Expanded && state.homeOpen) {
        scene.label = "Home";
        scene.tileRadius = std::min(nestedRadius, tokens().tileHeight / 2);
        scene.tiles = homeTilesFor(state, contentSize, scene.tileRadius, now, scene.elements);
    }

    if (source.content) {
        scene.label = source.content->label;
        scene.activityId = source.activityId;
        // A dialog over this card takes the content's place, at the card's
        // size: nothing morphs when a question opens or closes (decision 019).
        Content asked;
        if (const Dialog* d = scene.mode == Mode::Expanded ? dialogOver(state, source.activityId) : nullptr) {
            asked = dialogContent(*d);
            source.content = &asked;
            scene.label = d->title;
        }
        for (const SlotFrame& slot : slotFrames(scene.mode, contentSize, *source.content))
            placeSlot(slot, source, nestedRadius, now, scene.elements, measureText);
    }
    scene.tabs = tabStripFor(state, scene.size, nestedRadius, measureText, scene.elements);
    return scene;
}

float pillCornerRadius(Vec2 size)
{
    return std::min(std::min(size.x, size.y) / 2, tokens().maxCornerRadius);
}

Rect interactiveBounds(const Scene& scene)
{
    Rect pill{ scene.offset.x, scene.offset.y, scene.size.x, scene.size.y };
    if (!scene.bubble) return pill;
    const Rect& b = scene.bubble->frame;
    float left = std::min(pill.x, b.x), top = std::min(pill.y, b.y);
    float right = std::max(pill.x + pill.width, b.x + b.width), bottom = std::max(pill.y + pill.height, b.y + b.height);
    return { left, top, right - left, bottom - top };
}

const Tile* tileAt(const Scene& scene, Vec2 point)
{
    for (const Tile& tile : scene.tiles)
        if (tile.frame.contains(point)) return &tile;
    return nullptr;
}

std::optional<Ms> sceneDeadline(const Scene& scene)
{
    std::optional<Ms> next = scene.clockTick;
    std::vector<const Placed*> all;
    for (const Placed& p : scene.elements) all.push_back(&p);
    if (scene.bubble)
        for (const Placed& p : scene.bubble->elements) all.push_back(&p);
    for (const Placed* item : all) {
        const Placed& p = *item;
        if (!p.ticking || !p.ticking->playing || p.ticking->rate <= 0) continue;
        long long position = p.ticking->positionAt(scene.now).count();
        if (p.ticking->duration > Ms{ 0 } && position >= p.ticking->duration.count()) continue;
        // Playback ms until the shown second changes, for elapsed or remaining labels.
        long long remaining = p.ticking->duration.count() - position;
        long long untilElapsed = 1000 - position % 1000;
        long long untilRemaining = remaining > 0 ? remaining % 1000 + 1 : untilElapsed;
        long long playbackMs = std::min(untilElapsed, untilRemaining);
        Ms wallMs{ static_cast<long long>(std::ceil(static_cast<double>(playbackMs) / p.ticking->rate)) };
        Ms at = scene.now + wallMs;
        if (!next || at < *next) next = at;
    }
    return next;
}

} // namespace cup
