#pragma once
// What ranking needs to know about where a hit is (decision 075): how deep
// its folder is (shorter paths first) and whether it's inside a noise folder
// (WinSxS, node_modules: last). Both mean walking up the hit's folders, and
// hits share folders, so each folder's answer is worked out once per search
// and remembered. Pure, tested through search().

#include "indexer/file_index.h"

#include <cstdint>
#include <vector>

namespace cup::indexer {

class FolderMemo {
public:
    explicit FolderMemo(const FileIndex& index) : index_(index), known_(index.slots(), 0) {}

    struct Place {
        std::uint16_t depth = 0; // folders above it: 1 for C:\x.txt, 3 for C:\a\b\x.txt
        bool noise = false;      // one of them is a noise folder
    };
    Place of(std::uint32_t record);

private:
    Place folder(std::uint32_t folder); // a folder's own place: its depth, and noise at or above it

    const FileIndex& index_;
    std::vector<std::uint16_t> known_;  // per record: 0 unknown, else (depth + 1) | kNoiseBit (16 bits: stays in cache)
    std::vector<std::uint32_t> passed_; // reused: one walk at a time
};

} // namespace cup::indexer
