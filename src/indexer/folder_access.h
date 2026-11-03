#pragma once
// Which results a user may see (decision 075): the service reads every
// user's file names, so it shows a name only if the asking user could list
// the folder it's in. Checking means touching the disk as that user, so
// answers are kept per folder for a while. Pure: the check itself is handed
// in, and time is passed in.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

namespace cup::indexer {

// How long an answer is trusted: a permission change shows within this.
constexpr std::int64_t kFolderAccessMs = 5 * 60 * 1000;
// More folders than this and the cache starts over, so a long session of
// varied searches can't grow it without bound.
constexpr std::size_t kMaxCachedFolders = 20000;

class FolderAccess {
public:
    // canList(path): may the asking user list this folder ("C:\Docs")?
    explicit FolderAccess(std::function<bool(const std::string& folderPath)> canList) : canList_(std::move(canList)) {}

    // `volume` and `folder` (its record) name the folder; `folderPath` is
    // only built and checked when the answer isn't cached.
    bool visible(std::size_t volume, std::uint32_t folder, const std::string& folderPath, std::int64_t nowMs);
    std::size_t cached() const { return answers_.size(); }

private:
    std::function<bool(const std::string&)> canList_;
    std::unordered_map<std::uint64_t, bool> answers_;
    std::int64_t since_ = 0; // when answers_ was started
    bool started_ = false;
};

} // namespace cup::indexer
