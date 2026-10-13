#pragma once
// A text field's pixels (decisions 016, 030): its text or placeholder, the
// selection and the caret, scrolled sideways to keep the caret in view; and
// the other way round, where a click in it puts the caret. Both lay the text
// out the same way, so a click lands where the caret is drawn.

#include "core/content.h"
#include "platform/graphics_device.h"

#include <cstddef>

namespace cup::platform {

// Its background is a hover shape (element_visuals), not painted here.
void paintInput(ID2D1DeviceContext* dc, IDWriteFactory* dw, const TextInput& input, Vec2 size);

// The byte offset into input.value nearest `x` pixels from the field's left edge.
std::size_t caretAt(IDWriteFactory* dw, const TextInput& input, Vec2 size, float x);

} // namespace cup::platform
