#pragma once
// Which action, if any, a click on the pill triggers.

#include "core/layout.h"
#include "core/provider.h"

#include <optional>

namespace cup {

struct Hit {
    Action action;
    std::string key; // the Placed element that was hit, for press feedback
};

// `point` is in pill coordinates and tested against each element's hitFrame.
// Enabled buttons return their action; a seekable progress bar returns
// "seek" with the clicked fraction; a TextInput returns its actionId (the
// app focuses it rather than routing it, decision 016), and so does an
// editable Text, with its editAction (the app starts editing it, decision
// 030). Anything else returns nothing (the click is then a plain pill click).
// A floating toast (decision 071) is tested first: its button, or nothing
// for the rest of it, never what's under it. Over a row's open menu
// (decision 086): nothing; menuItemAt (core/menu.h) says which item.
std::optional<Hit> hitTest(const Scene& scene, Vec2 point);
// The same test over any elements (a window's page, decision 104), in their coordinates.
std::optional<Hit> hitIn(const std::vector<Placed>& elements, Vec2 point);

// Where along a progress bar `x` (pill coordinates) points, 0..1. Clamped, so
// a scrub dragged past either end (or off the pill) pins to it.
float seekFraction(const Placed& progress, float x);
// While the bar with key `bar` is dragged to `fraction` of its length: the
// scene with that bar's time labels (the ones in its row, on its timeline)
// showing the time under the pointer instead of playback's, so they follow
// the drag (decision 167). Anything else is as it was.
Scene scrubbedTimes(Scene scene, const std::string& bar, float fraction);

// The row under `point` ("<slot>/<row>", or "" for none): the element's under
// it, else the row whose band (its elements' height, anywhere across) it's
// in. Hover reveals a row's revealOnHover buttons anywhere along it, not
// only over an item (decision 063). Tabs aren't rows.
std::string rowAt(const Scene& scene, Vec2 point);

// Whether `point` lights `element` up under the pointer (decision 130): a button only over its own
// shape (its frame), not all of its click area, which reaches into the gaps round it; one lit by its
// row (Placed::litByRow) and anything else, wherever it takes a click.
bool lightsAt(const Placed& element, Vec2 point);

// `point` is in the top band of an expanded island (Scene::collapseZone,
// decision 108): a click there closes it, over tabs and buttons alike.
bool inCollapseZone(const Scene& scene, Vec2 point);

} // namespace cup
