#pragma once
// The blurred cover behind the island (decision 055). Pure: pixels in, pixels
// out. Done once per cover, when it's read, on a small copy, so the island
// shows a still picture: nothing is blurred or redrawn while music plays.

#include "core/content.h"

namespace cup {

// The cover shrunk to at most coverBackdropSize pixels on its long side,
// then box-blurred three times (close to a Gaussian). Opaque BGRA.
// An empty image for an empty or broken one.
ImageData blurredCover(const ImageData& cover);

} // namespace cup
