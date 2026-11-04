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

// Whether an item's own path ('\\' + its folders + its name) holds a path
// word (query.h's PathWord): worked out once per folder per search, as a
// folder holds it if its parent does. A path is only built for a folder whose
// name starts with the word's last part: the only way a match can end in it.
class PathMemo {
public:
    PathMemo(const FileIndex& index, std::string whole, std::string tail, bool anchored)
        : index_(index), whole_(std::move(whole)), tail_(std::move(tail)), anchored_(anchored), known_(index.slots(), 0),
          ends_(index.slots(), 0)
    {
        // "\\documents" (a leading separator only) has nothing before its last part.
        std::size_t last = whole_.find_last_of('\\');
        if (last != std::string::npos && last > 0) {
            std::size_t start = whole_.find_last_of('\\', last - 1);
            std::size_t begin = start == std::string::npos ? 0 : start + 1;
            before_ = whole_.substr(begin, last - begin);
        }
    }

    bool inside(std::uint32_t record);                    // a folder above it holds the word
    bool own(std::uint32_t record, std::string_view name); // its own path holds it, ending in its name

private:
    bool folderHolds(std::uint32_t folder);
    // '\\' + the folder's path + '\\' + the last part ends with the word: what
    // `own` asks of any item in it whose name starts with the last part. One
    // path built per folder asked, not one per item.
    bool endsHere(std::uint32_t folder);

    const FileIndex& index_;
    std::string whole_, tail_;
    std::string before_; // the part before the last ("report" in "x\\report\\photo"): a folder's name must end with it
    bool anchored_;
    std::vector<std::uint8_t> known_;   // per record: 0 unknown, 1 no, 2 yes
    std::vector<std::uint8_t> ends_;    // per folder: endsHere, the same way
    std::vector<std::uint32_t> passed_; // reused: one walk at a time
};

} // namespace cup::indexer
