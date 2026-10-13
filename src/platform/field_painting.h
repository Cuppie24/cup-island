#pragma once
// A text field's pixels (decisions 016, 043): its text or placeholder, the
// selection and the caret, scrolled sideways to keep the caret in view; and
// the other way round, where a click in it puts the caret. Both lay the text
// out the same way, so a click lands where the caret is drawn.

#include "core/content.h"
#include "platform/graphics_device.h"

#include <cstddef>
#include <optional>

namespace cup::platform {

// Its background is a hover shape (element_visuals), not painted here.
void paintInput(ID2D1DeviceContext* dc, IDWriteFactory* dw, const TextInput& input, Vec2 size);

// The byte offset into input.value nearest `at` (from the field's top left).
std::size_t caretAt(IDWriteFactory* dw, const TextInput& input, Vec2 size, Vec2 at);

// A wrapped field (decision 045): where the caret lands a line up (-1) or
// down (+1), at the same x. Nothing on the first or last line, or one line.
std::optional<std::size_t> caretOnLine(IDWriteFactory* dw, const TextInput& input, Vec2 size, int direction);

} // namespace cup::platform
