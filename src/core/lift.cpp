#include "core/lift.h"

#include "core/split.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// Lands where it is now, snapping (placement.h). Dropped because its mode
// can't stay lifted (it hid, a card opened), it's placed by the resting one.
void land(IslandState& s)
{
    Mode m = canLift(mode(s)) ? mode(s) : restingMode(s);
    s.spot = spotForDrop(liftedCentre(s), footprintOf(s, m), screenWidthOf(s));
    s.lift.reset();
}

} // namespace

void pressIsland(IslandState& s, Ms now)
{
    if (!canLift(mode(s)) || s.bubbleDrag || s.lift) return;
    s.pressedAt = now;
    s.pressDrag = {};
}

void dragIsland(IslandState& s, Vec2 drag)
{
    s.pressDrag = drag;
    if (!s.lift && isDrag(drag)) s.pressedAt.reset(); // moved before the hold: a click or a scrub, not a lift
}

void releaseIsland(IslandState& s)
{
    if (s.lift) land(s);
    s.pressedAt.reset();
    s.pressDrag = {};
}

void carryIsland(IslandState& s, float screenWidth, float pointerX)
{
    s.screenWidth = screenWidth;
    if (!s.lift) return; // let go on the way: it rests on the new monitor as anywhere
    // Positions on the old monitor mean nothing here (another origin, maybe
    // another scale): the pill's centre is the pointer, held on the screen.
    s.lift = pointerX;
    s.pressDrag = {};
}

void settleLift(IslandState& s, Ms now)
{
    Mode m = mode(s);
    if (s.lift && !canLift(m)) {
        land(s);
        s.pressedAt.reset();
    }
    if (s.pressedAt && !canLift(m)) s.pressedAt.reset();
    if (s.pressedAt && !s.lift && now >= *s.pressedAt + tokens().liftHold) {
        Placement at = placementOf(s, m);
        s.lift = at.pillX + pillWidth(s, m) / 2 - s.pressDrag.x; // no jump: the pill stays under the pointer
    }
}

std::optional<Ms> liftDeadline(const IslandState& s)
{
    if (!s.pressedAt || s.lift) return std::nullopt;
    return *s.pressedAt + tokens().liftHold;
}

bool canLift(Mode mode)
{
    return mode == Mode::Idle || mode == Mode::Compact || mode == Mode::Split || mode == Mode::Alert;
}

bool isLifted(const IslandState& s) { return s.lift.has_value(); }

float screenWidthOf(const IslandState& s) { return s.screenWidth > 0 ? s.screenWidth : tokens().canvas.x; }

Vec2 modeSize(Mode mode)
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

float pillWidth(const IslandState& s, Mode mode)
{
    const Tokens& t = tokens();
    bool pill = mode == Mode::Compact || mode == Mode::Split;
    if (pill && !s.activities.empty() && s.activities[0].content.widePill)
        return std::min(std::max(t.widePillWidth, t.compactSize.x), t.canvas.x); // never narrower than the pill, nor wider than the canvas
    return modeSize(mode).x;
}

Footprint footprintOf(const IslandState& s, Mode mode)
{
    const Tokens& t = tokens();
    return { pillWidth(s, mode), mode == Mode::Split ? splitGap() + t.bubbleSize : 0 };
}

Placement placementOf(const IslandState& s, Mode mode, std::optional<float> width)
{
    Footprint footprint = footprintOf(s, mode);
    if (width) footprint.pillWidth = *width;
    // Hanging under a file dialog, it's centred on that, not at its own spot (decision 181).
    Spot spot = s.picker ? Spot{ Dock::Free, s.picker->centre / screenWidthOf(s) } : s.spot;
    return placeAt(spot, footprint, screenWidthOf(s));
}

float liftedCentre(const IslandState& s)
{
    float base = s.lift.value_or(0);
    return clampLifted(base + s.pressDrag.x, pillWidth(s, mode(s)), screenWidthOf(s));
}

} // namespace cup
