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
// canvasSize is only applied at startup: the render layer is never resized.
bool loadTokens(const std::filesystem::path& file, bool startup);

} // namespace cup::app
