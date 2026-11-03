#include "indexer/folder_memo.h"

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

} // namespace cup::indexer
