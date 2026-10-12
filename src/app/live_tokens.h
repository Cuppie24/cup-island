#pragma once
// Loads tokens.json at startup and on every save, so sizes, timings and
// curves can be tuned while the island runs. The picked app theme's colours
// go on top (decision 022).

#include <filesystem>
#include <string>

namespace cup::app {

// tokens.json next to the exe if present (packaged builds), otherwise the
// source tree's assets/tokens.json (dev builds), so edits apply directly.
std::filesystem::path findTokensFile();

// Reads, validates and applies the file. Prints problems and keeps the
// previous values if anything is wrong. Returns true if tokens changed;
// if the canvas changed too (canvasSize, islandScale), the caller resizes
// the render layer.
bool loadTokens(const std::filesystem::path& file, bool startup);

// The display's scale, 1.0 = 100%: tokens.json is in logical pixels and
// every length is multiplied by it (scaleTokens). Set before the first load.
void setDisplayScale(float scale);

// The display's scale changed (Settings > Display > Scale, or another
// monitor): re-applies tokens.json at the new scale, canvasSize included.
// Returns true if tokens changed; the caller resizes the render layer.
bool rescaleTokens(const std::filesystem::path& file, float scale);

// The app theme (a name in tokens.json's "themes"): its colours go over the
// file's, now and after every reload. Returns true if tokens changed.
bool setTheme(const std::string& name);

} // namespace cup::app
