#pragma once
// Where the island sits along the top edge (decision 026). The user lifts it
// (press and hold) and drags it sideways; where it's let go becomes its Spot.
// Every mode is placed from that Spot here, so the edge rule holds in all of
// them: a side is flush with the screen edge (its flared corner gone) or at
// least flareRadius away (the whole flare shows), never in between.
// Pure: positions are render-layer pixels, which span the island's monitor.

#include "core/geometry.h"

#include <map>
#include <string>

namespace cup {

enum class Dock { Centre, Left, Right, Free };

struct Spot {
    Dock dock = Dock::Centre;
    float fraction = 0.5f; // Free: the pill's centre, as a fraction of the screen width
    bool operator==(const Spot&) const = default;
};

// The island's shapes, side by side: the pill, then (Split) a gap and the bubble.
struct Footprint {
    float pillWidth = 0;
    float rightOfPill = 0; // Split: splitGap() + bubbleSize; else 0
};

struct Placement {
    float pillX = 0;          // the pill's left edge
    bool flushLeft = false;   // the pill touches the screen's left edge: no left flare
    bool flushRight = false;  // the rightmost shape touches the right edge: no right flare
};

// Rests at `spot`, pushed onto the screen and off the half-flare band.
Placement placeAt(Spot spot, Footprint footprint, float screenWidth);

// Lifted: the pill's centre follows the pointer but stays on the screen.
float clampLifted(float pillCentre, float pillWidth, float screenWidth);

// Let go with the pill's centre here: snaps to the centre, to an edge, or stays.
Spot spotForDrop(float pillCentre, Footprint footprint, float screenWidth);

// Saved per monitor (display id -> Spot), one "<spot>\t<display id>" line each:
// <spot> is centre, left, right or the fraction. Unreadable lines are skipped.
std::map<std::string, Spot> parseSpots(const std::string& text);
std::string formatSpots(const std::map<std::string, Spot>& spots);

} // namespace cup
