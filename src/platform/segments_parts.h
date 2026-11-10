#pragma once
// Segments on the compositor (decision 093): a recessed track, a pill that
// slides to the picked option, and the labels twice: in the text colour on
// the track, and in the island's colour clipped to the pill, so each label
// turns dark exactly where the pill passes over it, even mid-slide.

#include "core/content.h"
#include "platform/win.h"

namespace cup::platform {

struct SegmentsParts {
    wuc::ContainerVisual visual{ nullptr };
    wuc::CompositionRoundedRectangleGeometry pill{ nullptr };    // the drawn pill
    wuc::CompositionRoundedRectangleGeometry pillClip{ nullptr }; // the same shape, clipping the dark labels
    wuc::CompositionColorBrush track{ nullptr }, pillBrush{ nullptr };
    wuc::CompositionDrawingSurface labels{ nullptr }; // every label, in the text colour
    wuc::CompositionDrawingSurface picked{ nullptr }; // every label, in the island's colour: shown inside the pill
};

// Its parts at `size`, the pill on `segments.picked`; the caller paints both surfaces.
SegmentsParts createSegments(wuc::Compositor const& c, wuc::CompositionDrawingSurface const& labels,
                             wuc::CompositionDrawingSurface const& picked, Vec2 size, const Segments& segments);

// Picked another option: the pill (and its clip) glide there, on the switch knob's curve.
void slideSegments(wuc::Compositor const& c, const SegmentsParts& parts, Vec2 size, const Segments& segments);

// A theme change: track and pill fade to the current colours.
void recolorSegments(const SegmentsParts& parts, const Segments& segments);

} // namespace cup::platform
