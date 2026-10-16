#pragma once
// The Shelf's file work on Windows (decision 061): copies into its store
// (%LOCALAPPDATA%\CupIsland\shelf\<item id>\) off the UI thread, with
// progress; opening things as Explorer would; checking that linked
// originals are still there. Everything comes back on the UI thread.

#include "providers/shelf/shelf_provider.h"

#include <filesystem>
#include <functional>

namespace cup::shelf_files {

std::filesystem::path storeFolder();

// At start: copies the app quit in the middle of are deleted (their items load as failed).
void clearUnfinished();

// `owner`: the window Windows' file picker belongs to (the add card, decision 064).
ShelfHooks hooks(std::function<void(std::function<void()>)> postToUi, void* owner);

} // namespace cup::shelf_files
