#include "indexer/folder_memo.h"

#include "indexer/query.h"

#include <algorithm>

namespace cup::indexer {
namespace {

constexpr std::uint16_t kNoiseBit = 1u << 15;
// Deep enough for any real tree; a loop in corrupt data can't spin for ever.
constexpr std::size_t kMaxDepth = 1024;

} // namespace

FolderMemo::Place FolderMemo::of(std::uint32_t record)
{
    if (!index_.used(record)) return {};
    std::uint32_t parent = index_.entry(record).parent;
    if (parent == record) return {};
    Place above = folder(parent);
    return { static_cast<std::uint16_t>(above.depth + 1), above.noise };
}

FolderMemo::Place FolderMemo::folder(std::uint32_t start)
{
    // Up to the first folder already known (or the root); then back down,
    // writing each folder passed: its parent's depth + 1, noise if it or any
    // folder above is a noise folder.
    passed_.clear();
    Place base;
    std::uint32_t at = start;
    for (std::size_t steps = 0; steps < kMaxDepth; ++steps) {
        if (at == kRootRecord || !index_.used(at)) break; // the volume's root: depth 0
        if (std::uint16_t k = known_[at]) {
            base = { static_cast<std::uint16_t>((k & ~kNoiseBit) - 1), (k & kNoiseBit) != 0 };
            break;
        }
        passed_.push_back(at);
        std::uint32_t parent = index_.entry(at).parent;
        if (parent == at) break;
        at = parent;
    }
    Place place = base;
    for (auto it = passed_.rbegin(); it != passed_.rend(); ++it) {
        place.depth = static_cast<std::uint16_t>(std::min<std::size_t>(place.depth + 1u, kMaxDepth));
        place.noise = place.noise || (index_.entry(*it).flags & kNoise);
        known_[*it] = static_cast<std::uint16_t>((place.depth + 1) | (place.noise ? kNoiseBit : 0));
    }
    return place;
}

bool PathMemo::own(std::uint32_t record, std::string_view name)
{
    // A match ending in this item's name follows the separator before it, so
    // the name must start with the last part, and then only its folder decides.
    if (name.size() < tail_.size() || !startsFolded(name, tail_)) return false;
    std::uint32_t parent = index_.entry(record).parent;
    return parent != record && endsHere(parent);
}

bool PathMemo::endsHere(std::uint32_t folder)
{
    bool top = folder == kRootRecord || !index_.used(folder);
    if (!top && ends_[folder]) return ends_[folder] == 2;
    // A folder whose name doesn't end with the part before the last can't
    // be where the word ends: no path to build (most folders, most searches).
    if (!before_.empty()) {
        std::string_view name = top ? std::string_view{} : index_.name(folder);
        bool fits = name.size() >= before_.size() && startsFolded(name.substr(name.size() - before_.size()), before_);
        if (!fits) {
            if (!top) ends_[folder] = 1;
            return false;
        }
    }
    std::string path = top ? std::string() : "\\" + index_.path(folder, "");
    path += "\\" + tail_;
    // Anchored ("d:\\x"): the whole path, from the drive's root, is the word.
    bool holds = anchored_ ? path.size() == whole_.size() && startsFolded(path, whole_)
                           : path.size() >= whole_.size() && startsFolded(std::string_view(path).substr(path.size() - whole_.size()), whole_);
    if (!top) ends_[folder] = holds ? 2 : 1;
    return holds;
}

bool PathMemo::inside(std::uint32_t record)
{
    if (!index_.used(record)) return false;
    std::uint32_t parent = index_.entry(record).parent;
    return parent != record && folderHolds(parent);
}

bool PathMemo::folderHolds(std::uint32_t start)
{
    // Up to the first folder already known (or the root); then back down:
    // a folder holds the word if its parent does, or its own path does.
    passed_.clear();
    bool holds = false;
    std::uint32_t at = start;
    for (std::size_t steps = 0; steps < kMaxDepth; ++steps) {
        if (at == kRootRecord || !index_.used(at)) break;
        if (std::uint8_t k = known_[at]) {
            holds = k == 2;
            break;
        }
        passed_.push_back(at);
        std::uint32_t parent = index_.entry(at).parent;
        if (parent == at) break;
        at = parent;
    }
    for (auto it = passed_.rbegin(); it != passed_.rend(); ++it) {
        holds = holds || own(*it, index_.name(*it));
        known_[*it] = holds ? 2 : 1;
    }
    return holds;
}

} // namespace cup::indexer
