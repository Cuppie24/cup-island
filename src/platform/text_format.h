#pragma once
// DirectWrite helpers shared by the painters (element_painting, field_painting).

#include "core/content.h"
#include "platform/graphics_device.h"

#include <string>

namespace cup::platform {

std::wstring widen(const std::string& utf8);

// The UI font for text of `pixelSize`: Segoe UI Variable's optical cut on Windows 11, Segoe UI on 10 (decision 024).
const wchar_t* textFamily(IDWriteFactory* dw, float pixelSize);

// One line, centred vertically in its box, ending in "…" if it doesn't fit.
winrt::com_ptr<IDWriteTextFormat> format(IDWriteFactory* dw, const wchar_t* family, float size, float weight, DWRITE_TEXT_ALIGNMENT align);

// Lines exactly `height` apart (core's lineHeight): wrapped text is drawn as
// tall as core laid it out (decision 045).
void setLineHeight(IDWriteTextFormat* format, float height);

winrt::com_ptr<ID2D1SolidColorBrush> brushOf(ID2D1DeviceContext* dc, const Color& color);

// The byte offset into `utf8` (the layout's text) nearest the point (x, y) in layout coordinates.
std::size_t hitOffset(IDWriteTextLayout* layout, const std::string& utf8, float x, float y);

} // namespace cup::platform
