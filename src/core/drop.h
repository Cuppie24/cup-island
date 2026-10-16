#pragma once
// What a drag carries and where it can land (decision 061). Pure data: the
// platform reads the drag into a summary, providers offer zones for it, core
// lays the zones out and says which one a drop landed on.

#include "core/content.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// What one dragged thing is. Shell items that aren't files never get here.
enum class DragKind { File, Folder, Text, Url };

struct DragItem {
    DragKind kind = DragKind::File;
    std::string name;                   // a file's name; text's first words; a link's title or address
    std::optional<std::uint64_t> bytes; // a file's size, once read (off the UI thread); text's length
    bool operator==(const DragItem&) const = default;
};

// The drag as providers see it while it's over the island: never paths.
struct DragSummary {
    std::vector<DragItem> items; // at most shelfMaxCheckedItems: a huge drag is only counted
    std::size_t total = 0;       // how many things the drag holds
    bool sizesKnown = false;     // files' sizes are read off the UI thread: until then, "Checking…"
    std::optional<std::uint64_t> freeBytes; // on the disk copies would go to, once known
    bool operator==(const DragSummary&) const = default;
};

// One dropped thing, handed only to the provider whose zone it landed on (PS §9).
struct DroppedItem {
    DragItem what;
    std::string path;  // File, Folder: where it is (UTF-8)
    std::string text;  // Text: the text; Url: the address
    std::string title; // Url: the page's title, if the drag had one
    bool operator==(const DroppedItem&) const = default;
};

// What a drop on a zone tells the drag's source. Never a move: on a move the
// source deletes the original (decision 061).
enum class DropEffect { None, Copy, Link };

// A place a drag can land, offered by a provider for this drag.
struct DropZone {
    std::string providerId; // set by the host
    std::string id;         // comes back with the drop; prefixed with the provider's id, like activities'
    std::string cardId;     // the card (panel or activity) that opens after a drop on it
    Glyph glyph = Glyph::Copy;
    std::string label;      // "Copy 2 · 1 too large, will be linked"
    std::string reason;     // a second, smaller line: why it's greyed, or what the label leaves out
    bool enabled = true;    // greyed: shown with its reason, but nothing lands on it
    DropEffect effect = DropEffect::Copy;
    bool operator==(const DropZone&) const = default;
};

} // namespace cup
