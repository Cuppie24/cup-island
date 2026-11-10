#pragma once
// Segments' geometry (decision 093): how wide the control is, where each
// option sits, which one a point is on. Shared by layout, clicks, the keys
// and the renderer, so they always agree.

#include "core/layout.h"

#include <optional>
#include <string>

namespace cup {

// An option's label as drawn and measured: caption size, centred.
Text segmentText(const Segments& segments, std::size_t option);

// The control's width: every option as wide as the widest label plus
// segmentPadding a side, all inside segmentInset.
float segmentsWidth(const Segments& segments, const MeasureText& measure);

// Option `option`'s pill within the control's `frame`: equal shares, inset.
Rect segmentFrame(const Segments& segments, Rect frame, std::size_t option);

// The option at `x` (pill coordinates) in a control at `frame`: the nearest one, even off its ends.
std::optional<std::size_t> segmentAt(const Segments& segments, Rect frame, float x);

// What a click on `option` sends: the control's actionId and its index.
std::string segmentAction(const Segments& segments, std::size_t option);

} // namespace cup
