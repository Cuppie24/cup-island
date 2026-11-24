#pragma once
// A line of text that changes as a whole (Text::drops, decision 180): the
// line being sung. The new line falls in from above as the old one drops
// out below, or (Text::rises) comes up from below as the old one leaves at
// the top; clipped to the line's frame, like a rolling time's digits
// (rolling_text) but one sprite for the whole line. Composition runs it: no
// app work per frame (TR §5).

#include "core/content.h"
#include "platform/graphics_device.h"

namespace cup::platform {

struct DroppingText {
    wuc::ContainerVisual visual{ nullptr }; // clips its lines to itself
    wuc::SpriteVisual line{ nullptr };      // the one shown; one on its way out is the container's alone
    wuc::CompositionDrawingSurface surface{ nullptr };
    Text text;         // what's shown
    // A clickable line (Text::actionId): a soft pill behind it, shown under the pointer, so it reads as something to click.
    wuc::ShapeVisual hover{ nullptr };
    wuc::CompositionColorBrush hoverBrush{ nullptr };
    float lineWidth = 0; // > 0: the line is wider than the frame and scrolls through it (Text::scrollUntil): its surface's width
    bool grey = false; // drawn without colour: a muted tab's (decision 072)
};

// Built and painted, `size` the text's frame (whole pixels).
// `now`: app time, for a long line's scroll (Text::scrollUntil).
DroppingText createDroppingText(wuc::Compositor const& c, GraphicsDevice& device, const Text& text, Vec2 size, bool grey, Ms now);
// Shows `text`: other words drop in; the same words in another look are
// redrawn in place. False if the GPU was lost (the caller repaints).
bool dropTo(wuc::Compositor const& c, GraphicsDevice& device, DroppingText& dropping, const Text& text, Vec2 size, Ms now);
// The shown line painted again, in place: new colours, greyed or not, a lost device.
bool repaintDropping(GraphicsDevice& device, DroppingText& dropping, Vec2 size);

} // namespace cup::platform
