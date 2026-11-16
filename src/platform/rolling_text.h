#pragma once
// A Hero time whose characters roll as they change (decision 107): each
// character is its own sprite, and a changed one drops out below as the new
// one falls in from above, clipped to the line, like a flip clock's digits.
// Composition runs it: no app work per frame (TR §5).

#include "core/content.h"
#include "platform/graphics_device.h"

#include <string>
#include <vector>

namespace cup::platform {

struct RollingCell {
    wuc::SpriteVisual sprite{ nullptr };
    wuc::CompositionDrawingSurface surface{ nullptr };
    std::wstring value; // its character
    float left = 0;     // its advance's left edge in the text's frame
};

struct RollingText {
    wuc::ContainerVisual visual{ nullptr }; // clips its cells to itself: the line, `lead` wider on each side
    std::vector<RollingCell> cells;
    Text text;        // what's shown
    float lead = 0;   // the visual starts this far left of the frame: room for a glyph's overhang
    bool grey = false; // drawn without colour: a muted tab's (decision 072)
};

// How far past its advance a character's ink may reach: the room each cell keeps on both sides.
float rollingLead(const Text& text);
// Built and painted, `size` the text's frame (whole pixels).
RollingText createRollingText(wuc::Compositor const& c, GraphicsDevice& device, const Text& text, Vec2 size, bool grey);
// Shows `text`: characters that changed roll, the rest stay put (all of them,
// if its length changed). False if the GPU was lost (the caller repaints).
bool rollTo(wuc::Compositor const& c, GraphicsDevice& device, RollingText& rolling, const Text& text, Vec2 size);
// Every cell painted again, in place: new colours, greyed or not, a lost device.
bool repaintRolling(GraphicsDevice& device, RollingText& rolling, Vec2 size);

} // namespace cup::platform
