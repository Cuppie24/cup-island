#pragma once
// The daydream (decision 050): hold Home's system-wide shortcut and, instead
// of Home opening and closing, the island lets go of the screen edge and
// floats, spinning and wobbling, until the keys are let go. The press itself
// still opens Home at once (no lag on a normal press); held for daydreamHold,
// Home closes again and the show starts. Nothing on the island takes a click
// meanwhile: it isn't where its interaction window is (decision 001).

#include "core/island.h"

#include <optional>

namespace cup {

void holdHomeShortcut(IslandState& s, Ms now); // pressed, and still down
void releaseHomeShortcut(IslandState& s);      // let go: the island lands
// Every event: a hold long enough starts the daydream (true: it just did, so
// the caller closes Expanded); hiding ends it.
bool settleDaydream(IslandState& s, Ms now);
// When a held shortcut starts the daydream, if one is held.
std::optional<Ms> daydreamDeadline(const IslandState& s);
// While daydreaming, clicks and Home opening are ignored: it's in the air.
bool ignoredWhileDaydreaming(const Event& event);

} // namespace cup
