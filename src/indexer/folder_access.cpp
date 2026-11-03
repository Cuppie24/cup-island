#include "indexer/folder_access.h"

namespace cup::indexer {

bool FolderAccess::visible(std::size_t volume, std::uint32_t folder, const std::string& folderPath, std::int64_t nowMs)
{
    // All answers expire together: simpler than per-entry times, and a
    // re-check costs one open of a folder.
    if (!started_ || nowMs - since_ >= kFolderAccessMs || nowMs < since_ || answers_.size() >= kMaxCachedFolders) {
        answers_.clear();
        since_ = nowMs;
        started_ = true;
    }
    std::uint64_t key = (static_cast<std::uint64_t>(volume) << 32) | folder;
    auto found = answers_.find(key);
    if (found != answers_.end()) return found->second;
    bool allowed = canList_(folderPath);
    answers_.emplace(key, allowed);
    return allowed;
}

} // namespace cup::indexer
