#pragma once
// Lifting the island and moving it along the top edge (decision 026). Press
// and hold the pill (or an alert) for liftHold without moving: it lifts,
// follows the pointer sideways only, and lands where it's let go, snapping
// to the centre or an edge (placement.h). Expanded never lifts. Shared by
// reduce() (the gesture) and layout() (where every mode is drawn).

#include "core/island.h"
#include "core/placement.h"

#include <optional>

namespace cup {

// --- The gesture: reduce() calls these -------------------------------------
void pressIsland(IslandState& s, Ms now);
void dragIsland(IslandState& s, Vec2 drag);
void releaseIsland(IslandState& s);
// Every event: lifts a press held long enough; drops (where it is) a lift
// whose mode can't be lifted any more, e.g. Home opened or the island hid.
void settleLift(IslandState& s, Ms now);
// When a held press will lift, if one is waiting.
std::optional<Ms> liftDeadline(const IslandState& s);

// --- Where things are: layout() and the bubble's drag rule ------------------
bool canLift(Mode mode); // the pill and alerts; never Expanded (or Receptive, Hidden)
bool isLifted(const IslandState& s);
float screenWidthOf(const IslandState& s); // the canvas width until the platform says
Vec2 modeSize(Mode mode);                  // the pill's size per mode, before content (a tall card) grows it
Footprint footprintOf(Mode mode);          // the shapes side by side at rest: Split adds the bubble
Placement placementOf(const IslandState& s, Mode mode); // at rest, from s.spot
float liftedCentre(const IslandState& s);  // lifted: the pill's centre x, on the screen

} // namespace cup
