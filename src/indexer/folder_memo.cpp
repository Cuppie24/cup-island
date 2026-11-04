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
    const std::string& last = parts_.back();
    return name.size() >= last.size() && startsFolded(name, last) && above(record) + 1 >= parts_.size();
}

bool PathMemo::inside(std::uint32_t record)
{
    return above(record) >= parts_.size();
}

std::size_t PathMemo::above(std::uint32_t record)
{
    if (!index_.used(record)) return 0;
    std::uint32_t parent = index_.entry(record).parent;
    return parent == record ? 0 : folder(parent);
}

std::size_t PathMemo::folder(std::uint32_t start)
{
    // Up to the first folder already known (or the root); then back down,
    // each folder taking the next part if its name starts with it. Matching
    // the earliest part a folder can take is never worse: gaps are allowed.
    passed_.clear();
    std::size_t matched = 0;
    std::uint32_t at = start;
    for (std::size_t steps = 0; steps < kMaxDepth; ++steps) {
        if (at == kRootRecord || !index_.used(at)) break;
        if (std::uint8_t k = known_[at]) {
            matched = k - 1u;
            break;
        }
        passed_.push_back(at);
        std::uint32_t parent = index_.entry(at).parent;
        if (parent == at) break;
        at = parent;
    }
    for (auto it = passed_.rbegin(); it != passed_.rend(); ++it) {
        if (matched < parts_.size() && startsFolded(index_.name(*it), parts_[matched])) ++matched;
        known_[*it] = static_cast<std::uint8_t>(matched + 1);
    }
    return matched;
}

} // namespace cup::indexer
