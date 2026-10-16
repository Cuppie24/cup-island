#pragma once
// What's on the Shelf (decision 061): links to originals and copies kept in
// the shelf's store. Pure: the list, the changes to it, and how it's saved
// (Host::save, decision 028). The files themselves are the app's (ShelfHooks).

#include "core/drop.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cup::shelf {

enum class Kind { Link, Copy };
// Copying: its copy is under way. Failed: it didn't finish (why: `problem`).
// Missing: a link whose original is gone, or its drive is offline.
enum class State { Ready, Copying, Failed, Missing };

struct Item {
    std::uint64_t id = 0;   // stable while the app runs, and kept: buttons and the store's folders name it
    std::string name;       // shown
    Kind kind = Kind::Link;
    State state = State::Ready;
    DragKind what = DragKind::File;
    std::string path;       // Link: the original; Copy: the copy in the store, once made
    std::string source;     // Copy: where it was copied from, if it has a place on disk ("Keep as link")
    std::uint64_t bytes = 0; // Copy: what it takes in the store
    float progress = 0;     // Copying: 0..1
    std::string problem;    // Failed: why
    bool operator==(const Item&) const = default;
};

class Items {
public:
    const std::vector<Item>& all() const { return items_; } // oldest first
    const Item* find(std::uint64_t id) const;

    std::uint64_t add(Item item);            // gives it the next id; returns it
    bool remove(std::uint64_t id);
    void clear();
    bool setProgress(std::uint64_t id, float fraction);
    // A copy finished: there, taking `bytes`; or it failed, saying why.
    bool copied(std::uint64_t id, const std::string& stored, std::uint64_t bytes);
    bool failed(std::uint64_t id, const std::string& problem);
    bool keepAsLink(std::uint64_t id);       // a failed copy becomes a link to its source
    bool setMissing(std::uint64_t id, bool missing); // a link's original is gone, or back

    std::uint64_t storeUsed() const;         // every copy's bytes, finished or not
    bool anyLinks() const;

    // One line per item, tab-separated. A copy still under way loads as failed:
    // the app quit before it finished, and its half-made file is gone.
    std::string save() const;
    static Items load(const std::string& saved);

private:
    Item* at(std::uint64_t id);
    std::vector<Item> items_;
    std::uint64_t nextId_ = 1;
};

} // namespace cup::shelf
