#pragma once
// The host's toast (decision 071): a short note with one action floating over
// the bottom of its card, above its content, gone by itself after a while. Pure.

#include "core/island.h"
#include "core/layout.h"
#include "core/slot_layout.h"

#include <optional>

namespace cup {

// The toast on this card, if one is up.
const Toast* toastOn(const IslandState& state, const std::string& activityId);

// Where it floats: a pill as wide as its text and button (at most the card's
// width less its padding), centred, its bottom `bottom` (the content's
// bottom edge); its text quiet and its action a button in it.
ToastScene placeToast(const Toast& toast, const Source& source, Vec2 cardSize, float bottom, Ms now, const MeasureText& measure);

} // namespace cup
