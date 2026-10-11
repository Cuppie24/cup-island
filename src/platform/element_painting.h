#pragma once
// Pixels for content items: Direct2D/DirectWrite drawing into a surface of
// `size` pixels. Stateless apart from cached font lookups.

#include "core/content.h"
#include "platform/graphics_device.h"

namespace cup::platform {

void paintText(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, Vec2 size);
// Width of `text` on a single line, untrimmed: decides whether marquee text has to swim.
float measureText(IDWriteFactory* dw, const Text& text);
// Swimming text: the string twice, `gap` apart, starting at x = `lead`, so
// scrolling one copy's width plus the gap loops seamlessly.
void paintMarquee(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, float width, float lead, float gap, float height);
// The "this toggle is on" dot under a glyph of `glyphSize` centred in `size`.
void paintToggleDot(ID2D1DeviceContext* dc, Vec2 size, float glyphSize);
void paintGlyph(ID2D1DeviceContext* dc, IDWriteFactory* dw, Glyph glyph, float fontSize, Vec2 size, const Color& color);
// A text button's label, centred.
void paintLabel(ID2D1DeviceContext* dc, IDWriteFactory* dw, const std::string& label, Vec2 size, const Color& color);
// A text field (decision 016): its background, the text (or the placeholder,
// dim) and, when focused, a caret after it.
void paintInput(ID2D1DeviceContext* dc, IDWriteFactory* dw, const TextInput& input, Vec2 size, float cornerRadius);
// Steps (decision 010): a dot per step, centred vertically; done and to-do
// ones in their token colours, the current one in `accent`.
void paintSteps(ID2D1DeviceContext* dc, const Steps& steps, Vec2 size, const Color& accent);
// Fills a rounded rect with the image scaled to cover it; a neutral tile if `data` is null.
void paintImage(ID2D1DeviceContext* dc, const ImageData* data, Vec2 size, float cornerRadius);

} // namespace cup::platform
