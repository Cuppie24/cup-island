#pragma once
// Pixels for content items: Direct2D/DirectWrite drawing into a surface of
// `size` pixels. Stateless apart from cached font lookups.

#include "core/content.h"
#include "platform/graphics_device.h"

namespace cup::platform {

void paintText(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, Vec2 size);
void paintGlyph(ID2D1DeviceContext* dc, IDWriteFactory* dw, Glyph glyph, float fontSize, Vec2 size, float alpha);
// Fills a rounded rect with the image scaled to cover it; a neutral tile if `data` is null.
void paintImage(ID2D1DeviceContext* dc, const ImageData* data, Vec2 size, float cornerRadius);

} // namespace cup::platform
