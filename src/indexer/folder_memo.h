#pragma once
// What ranking needs to know about where a hit is (decision 075): how deep
// its folder is (shorter paths first) and whether it's inside a noise folder
// (WinSxS, node_modules: last). Both mean walking up the hit's folders, and
// hits share folders, so each folder's answer is worked out once per search
// and remembered. Pure, tested through search().

#include "indexer/file_index.h"

#include <cstdint>
#include <string>
#include <string_view>
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

// A path word's parts against an item's folders (query.h's PathWord): how
// many of the parts its folders, from the top down, have matched in order
// (each part the start of one folder's name, gaps allowed). Worked out once
// per folder per search, from its parent's count; no paths are built.
class PathMemo {
public:
    PathMemo(const FileIndex& index, std::vector<std::string> parts)
        : index_(index), parts_(std::move(parts)), known_(index.slots(), 0)
    {
    }

    // Its name starts with the last part, and its folders hold the others in
    // order: "projects/cwatis" for D:\\Projects\\claude-micros-wiki\\cwatis.
    bool own(std::uint32_t record, std::string_view name);
    // Its folders hold every part: it's inside a match (its contents).
    bool inside(std::uint32_t record);

private:
    std::size_t above(std::uint32_t record); // parts matched by the folders above it
    std::size_t folder(std::uint32_t folder); // ...and by the folder itself, too

    const FileIndex& index_;
    std::vector<std::string> parts_;
    std::vector<std::uint8_t> known_;   // per record: 0 unknown, else parts matched + 1
    std::vector<std::uint32_t> passed_; // reused: one walk at a time
};

} // namespace cup::indexer
