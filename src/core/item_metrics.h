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
// A wavy bar's height: its wave's swing or the pin at its head, whichever is taller.
float waveHeight();
float waveSwing(); // the line, swinging progressWaveAmplitude to each side
// The wave's line, left to right: points along a sine from x = 0 to `width`,
// around y = `middle`, starting on it and rising first. Close enough together
// to read as a curve. Nothing for a wave with no length.
std::vector<Vec2> wavePoints(float width, float middle);
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
