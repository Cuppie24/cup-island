#pragma once
// How big content items are (PS §5.1): heights, fixed widths, text sizes.
// Shared by slot layout and lists (decision 029).

#include "core/layout.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

float lineHeight(TextStyle style); // a line of text in this style
float fontSize(TextStyle style);   // its text's size, px (tokens)
// The style of a Text::detail line under text in `style` (decision 079).
TextStyle detailStyleOf(TextStyle style);
bool isRing(const Item& item);
bool isWavy(const Item& item); // a Progress bar with `wavy` (decision 167)
// A wavy bar's height: its line's swing, or its knob grown under the pointer if it has one.
float waveHeight(bool seekable = false);
float waveSwing(); // the line, swinging progressWaveAmplitude to each side
// The wave's line as it is `phase` (radians) into its ripple, in its head's
// own coordinates: x from -length (its start) to 0 (its head), around
// y = `middle`. A sine whose swing shrinks to nothing over the last
// progressWaveTaper before the head, so it meets the head on the bar's
// line; as `phase` grows the ripples run towards the head. `swing` 0: flat.
// The same number of points whatever the phase, so one line can turn into another.
std::vector<Vec2> wavePoints(float length, float middle, float phase, float swing = 1);
// Step i's length on screen, from its StepLength (decision 107).
float stepWidth(const Steps& steps, int i);
// A primary button's circle (decision 064) and the glyph in it; a compact
// one is the main action in a row of controls, media's ▶ (decision 094).
float primarySize(const Button& button);
float primaryGlyphOf(const Button& button);
// A Text's width on one line: its value's, or its detail's under it if wider (decision 090).
float textWidth(const Text& text, const MeasureText& measure);
// A Dial's diameter: its widest time and its label, as a block, fit inside the circle, dialInset in (decision 065).
float dialDiameter(const Dial& dial, const MeasureText& measure);

// Natural height of an item. Images fill the slot's square; a ring is
// `ringSide` (its row's height, or the slot's square when alone).
float heightOf(const Item& item, Rect slot, float ringSide, const MeasureText& measure = {});

// Fixed width, or nothing for items that stretch (Text, TextInput, a bar).
std::optional<float> fixedWidthOf(const Item& item, float height, const MeasureText& measure);

} // namespace cup
