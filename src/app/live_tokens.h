#pragma once
// Loads tokens.json at startup and on every save, so sizes, timings and
// curves can be tuned while the island runs.

#include <filesystem>

namespace cup::app {

// tokens.json next to the exe if present (packaged builds), otherwise the
// source tree's assets/tokens.json (dev builds), so edits apply directly.
std::filesystem::path findTokensFile();

// Reads, validates and applies the file. Prints problems and keeps the
// previous values if anything is wrong. Returns true if tokens changed.
// canvasSize is only applied at startup (and on a display-scale change):
// the render layer isn't resized for an edit.
bool loadTokens(const std::filesystem::path& file, bool startup);

// The display's scale, 1.0 = 100%: tokens.json is in logical pixels and
// every length is multiplied by it (scaleTokens). Set before the first load.
void setDisplayScale(float scale);

// The display's scale changed (Settings > Display > Scale, or another
// monitor): re-applies tokens.json at the new scale, canvasSize included.
// Returns true if tokens changed; the caller resizes the render layer.
bool rescaleTokens(const std::filesystem::path& file, float scale);

} // namespace cup::app
