// Where the island rests along the top edge (decision 026): the edge rule,
// snapping, and the saved spots. The gesture's rules are in lift.scenario.

#include "core/layout.h"
#include "core/lift.h"
#include "core/placement.h"
#include "core/tokens.h"
#include "expect.h"

namespace cup::test {

void runPlacementTests()
{
    const Tokens& t = tokens();
    const float W = 1000, flare = t.flareRadius;
    Footprint pill{ 160, 0 }, split{ 260, t.bubbleGap + t.bubbleSize };

    // Docks.
    expect(placeAt({ Dock::Centre }, pill, W).pillX == 420, "centre: the pill centred");
    expect(placeAt({ Dock::Centre }, split, W).pillX == 370, "centre in Split: the pill centred, the bubble beside it");
    Placement left = placeAt({ Dock::Left }, pill, W), right = placeAt({ Dock::Right }, split, W);
    expect(left.pillX == 0 && left.flushLeft && !left.flushRight, "left: flush, no left flare");
    expect(right.pillX + 260 + split.rightOfPill == W && right.flushRight && !right.flushLeft, "right in Split: the bubble flush");

    // The edge rule: a gap is 0 or at least the flare, never in between, in every mode.
    for (float x = 0; x <= 1; x += 0.001f) {
        for (Footprint f : { pill, split, Footprint{ t.expandedSize.x, 0 } }) {
            Placement p = placeAt({ Dock::Free, x }, f, W);
            float gapLeft = p.pillX, gapRight = W - (p.pillX + f.pillWidth + f.rightOfPill);
            bool ok = (gapLeft == 0 || gapLeft >= flare) && (gapRight == 0 || gapRight >= flare) && gapLeft >= 0 && gapRight >= 0;
            if (!ok) {
                expect(false, "free spot leaves a half flare or runs off the screen");
                x = 2;
                break;
            }
        }
    }
    Placement nearLeft = placeAt({ Dock::Free, (80 + flare / 2) / W }, pill, W);
    expect(nearLeft.pillX == 0 && nearLeft.flushLeft, "half a flare from the edge: pushed flush");
    expect(placeAt({ Dock::Free, (80 + flare) / W }, pill, W).pillX == flare, "a whole flare away: stays");

    // Lifted: on the screen, nothing more.
    expect(clampLifted(-50, 160, W) == 80 && clampLifted(2000, 160, W) == 920 && clampLifted(300, 160, W) == 300, "lifted: held on the screen");

    // Drops snap: the centre zone, then the edges (never narrower than the flare).
    expect(spotForDrop(500 + t.centreSnap, pill, W).dock == Dock::Centre, "centre snap, inclusive");
    expect(spotForDrop(80 + t.edgeSnap, pill, W).dock == Dock::Left, "edge snap");
    expect(spotForDrop(W - 40 - 130 - t.edgeSnap, split, W).dock == Dock::Right, "edge snap counts the bubble");
    Spot free = spotForDrop(300, pill, W);
    expect(free.dock == Dock::Free && free.fraction == 0.3f, "elsewhere: where it was let go");

    // Saved per monitor.
    std::map<std::string, Spot> spots{ { "", { Dock::Left } }, { "\\\\?\\DISPLAY#DEL", { Dock::Free, 0.25f } }, { "B", { Dock::Right } } };
    expect(parseSpots(formatSpots(spots)) == spots, "spots survive a save and a load");
    auto read = parseSpots("centre\tA\r\nbogus\tB\n0.4\tC\n1.5\tD\nno tab\n");
    expect(read.size() == 2 && read["A"].dock == Dock::Centre && read["C"].fraction == 0.4f, "unreadable lines are skipped");

    // layout(): the pill where it rests; flush sides marked; lifted below the edge.
    IslandState s;
    s = reduce(s, ScreenResized{ W }, Ms{ 0 });
    s = reduce(s, SpotRestored{ { Dock::Left } }, Ms{ 0 });
    Scene docked = layout(s, Ms{ 0 });
    expect(docked.offset == Vec2{ 0, 0 } && docked.flushLeft && !docked.flushRight && !docked.lifted, "docked left in Idle");
    s = reduce(s, ActivityStarted{ { .id = "a" } }, Ms{ 0 });
    s = reduce(s, ActivityStarted{ { .id = "b" } }, Ms{ 0 });
    s = reduce(s, SpotRestored{ { Dock::Right } }, Ms{ 0 });
    Scene splitRight = layout(s, Ms{ 0 });
    expect(splitRight.bubble && splitRight.bubble->frame.x + splitRight.bubble->frame.width == W && splitRight.flushRight,
           "docked right in Split: the bubble at the edge");
    s = reduce(s, IslandPressed{}, Ms{ 0 });
    s = reduce(s, Tick{}, t.liftHold);
    Scene lifted = layout(s, t.liftHold);
    expect(lifted.lifted && lifted.offset.y == t.liftDrop && !lifted.bubble && !lifted.flushRight,
           "lifted: below the edge, the bubble tucked into the pill");
    expect(lifted.offset.x == splitRight.offset.x, "lifting doesn't move it sideways");
}

} // namespace cup::test
