#pragma once
// Numbers that tick (decision 094). Figures all one width, so a playing
// time's digits don't shift as they change, and a line of them centred on
// its digits, so a time beside a progress bar sits on the bar's line.

#include "platform/graphics_device.h"

namespace cup::platform {

// OpenType tabular figures ("tnum") for the layout's first `length`
// characters. Segoe UI's figures are tabular already; this keeps them so in
// any font with both kinds.
void useTabularFigures(IDWriteFactory* dw, IDWriteTextLayout* layout, UINT32 length);

// How far down to draw a one-line layout, centred in its box, so that the
// middle of its digits (half the cap height over the baseline) is at the
// box's middle, not the middle of its line (ascent + descent). 0 if the
// font can't be looked up.
float figureNudge(IDWriteFactory* dw, IDWriteTextLayout* layout);

} // namespace cup::platform
