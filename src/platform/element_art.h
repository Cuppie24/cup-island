#pragma once
// Clickable art (an Image with an action: the media art opens its app). Under
// the pointer it dims and an "open" glyph springs in, so it reads as a
// button without looking like one at rest (decision 002: content-level hover).

#include "core/tokens.h"
#include "platform/win.h"

namespace cup::platform {

struct ArtParts {
    wuc::ContainerVisual visual{ nullptr };
    wuc::Visual overlay{ nullptr }; // dim + glyph; opacity 0 until hovered
    wuc::Visual glyph{ nullptr };   // springs from small to full size on hover
    wuc::CompositionColorBrush dim{ nullptr }; // artHoverColor, faded on a recolour
};

// `art` holds the picture, `glyph` the "open" glyph; both are frame-sized.
ArtParts createActionArt(wuc::Compositor const& c, Vec2 frame, float cornerRadius,
                         wuc::CompositionDrawingSurface const& art, wuc::CompositionDrawingSurface const& glyph);

// Hover in: the overlay fades in and the glyph springs up (with the press
// release's overshoot). Hover out: both settle back.
void showArtOverlay(wuc::Compositor const& c, const ArtParts& parts, bool on);

} // namespace cup::platform
