#pragma once
// Liquid glass (decision 165): what the shader that bends the desktop needs
// to know about the island's shapes. The outline on screen is cut exactly by
// the compositor; this only says where the rim's bend goes, so a pixel or
// two of difference mid-morph doesn't show. Pure: time is passed in.

#include "core/content.h"
#include "core/geometry.h"
#include "core/motion.h"
#include "core/tokens.h"

namespace cup {

// One shape as the shader sees it: a rounded rectangle, render-layer pixels.
struct LiquidShape {
    Rect frame;
    float radius = 0;
    // On the screen edge its top corners flare out into the edge (decision
    // 025): the body's real top (the frame's is carried past it), and each
    // flare's size; 0: none on that side.
    float top = 0;
    float leftEar = 0, rightEar = 0;
    bool operator==(const LiquidShape&) const = default;
};

// `body` (a pill's or a bubble's bounds) as a shape: the island's corner
// radius, and while it hangs from the screen edge (`attached`), its top
// carried far enough past that edge that no rim bends along it: only the
// sides and the bottom are a pane's lip, and those sides curve out into the
// edge where the body has a flared corner (`leftEar`, `rightEar`: decision
// 026 takes one away where it's flush with the screen's side), so the
// glass's outline follows them. Nothing (an empty frame) if it has no size.
LiquidShape liquidShape(Rect body, bool attached, bool leftEar, bool rightEar, const Tokens& t);

// Liquid glass is picked and the island is glass. (Its shiny edges once went
// by this; they're on either glass now, decision 175.)
bool liquidPicked(const Tokens& t);

// ...and its picture of the desktop is drawn (decision 172): only while a
// card is open. Collapsed, the pill is the frosted glass, which costs
// nothing; the capture behind liquid glass copies the whole screen however
// small the island is, and the island is collapsed most of the time.
bool liquidDrawn(const Tokens& t, bool cardOpen);

// On liquid glass, a button with no fill of its own that still gets a shiny
// edge round its frame: a card's main controls beside its primary one
// (media's skips, Button::large). The small ones (a row's, the page row's)
// stay bare: a ring round each would be noise. Filled buttons shine with their fill.
bool liquidRing(const Button& button);

// Where a morph from `from` to `to` has got to, `elapsed` after it started:
// the same spring the compositor runs (springProgress), overshoot included.
Rect morphRect(Rect from, Rect to, const Motion& motion, Ms elapsed);

// The pixels a frame must draw for `shapes`: around them all, `margin` wider
// (their ears, their shadow's edge). Empty if there's nothing to draw.
Rect liquidBounds(const std::vector<LiquidShape>& shapes, float margin);

} // namespace cup
