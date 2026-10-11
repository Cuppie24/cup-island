#pragma once
// Event-driven file watching (no polling, TR §8): used to live-reload tokens.json.

#include <filesystem>
#include <functional>

namespace cup::platform {

// Calls onChange on a background thread whenever anything in the file's
// directory is written, created or renamed (editors often save via a temp
// file + rename, so watching the single file isn't enough). Expect several
// calls per save; callers should debounce and compare contents.
void watchDirectoryOf(const std::filesystem::path& file, std::function<void()> onChange);

} // namespace cup::platform
