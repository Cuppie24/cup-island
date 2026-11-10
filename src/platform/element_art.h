#pragma once
// Clickable art (an Image with an action: the media art opens its app). Under
// the pointer it dims and an "open" glyph springs in, so it reads as a
// button without looking like one at rest (decision 002: content-level hover).

#include "core/tokens.h"
#include "platform/win.h"

namespace cup::platform {

struct ArtParts {
    wuc::ContainerVisual visual{ nullptr };
    wuc::SpriteVisual picture{ nullptr }; // shows the art surface
    wuc::Visual overlay{ nullptr }; // dim + glyph; opacity 0 until hovered
    wuc::Visual glyph{ nullptr };   // springs from small to full size on hover
    wuc::CompositionColorBrush dim{ nullptr }; // artHoverColor, faded on a recolour
};

// `art` holds the picture, `glyph` the "open" glyph; both are frame-sized.
ArtParts createActionArt(wuc::Compositor const& c, Vec2 frame, float cornerRadius,
                         wuc::CompositionDrawingSurface const& art, wuc::CompositionDrawingSurface const& glyph);

// Raised art (Placed::raised, decision 094): a soft shadow under it, cast by
// a brushless sprite at the bottom of `container`, masked to `shape` (the
// art's rounded rect, drawn opaque) so it follows the corners, not the square.
// Blur, offset and colour from tokens; a recolour fades the colour (fadeColor).
wuc::DropShadow addArtShadow(wuc::Compositor const& c, wuc::ContainerVisual const& container, Vec2 frame,
                             wuc::CompositionDrawingSurface const& shape);

// Hover in: the overlay fades in and the glyph springs up (with the press
// release's overshoot). Hover out: both settle back.
void showArtOverlay(wuc::Compositor const& c, const ArtParts& parts, bool on);

} // namespace cup::platform
