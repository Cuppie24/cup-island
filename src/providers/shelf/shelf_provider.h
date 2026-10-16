#pragma once
// The Shelf (decision 061): files, text and links dropped on the island,
// kept as links to the originals or as copies in its own store. It offers
// two zones for a drag, Link and Copy, each saying what it would do; a drop
// lands on its card, which stays open showing what came in. Copying, opening
// and checking files are the app's (ShelfHooks), so this stays pure and is
// tested on Linux.

#include "core/provider.h"
#include "providers/shelf/drop_plan.h"
#include "providers/shelf/shelf_items.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cup {

// One thing to put in the shelf's store.
struct CopyJob {
    std::uint64_t id = 0;     // the item's: its folder in the store, so names never clash
    std::string name;         // the file's name there (safe_names.h)
    std::string source;       // a file to copy...
    std::string contents;     // ...or this to write (text, a link's .url)
    std::uint64_t maxBytes = 0; // a file found larger while copying fails: its size wasn't known before
    bool operator==(const CopyJob&) const = default;
};

struct ShelfHooks {
    // Copies or writes off the UI thread; `progress` (0..1) and `done` come on
    // the UI thread. done: where the copy is and its size, or why it failed.
    using Progress = std::function<void(float)>;
    using Done = std::function<void(const std::string& stored, std::uint64_t bytes, const std::string& problem)>;
    std::function<void(const CopyJob& job, Progress progress, Done done)> copy;
    std::function<void(const std::string& stored)> removeCopy; // a copy in the store; never an original
    std::function<void(const std::string& path)> open;         // in its app, as Explorer would
    // Which of these still exist, off the UI thread (a network drive can be slow); `done` on the UI thread.
    std::function<void(const std::vector<std::string>& paths, std::function<void(std::vector<bool> present)> done)> checkLinks;
};

class ShelfProvider : public Provider {
public:
    explicit ShelfProvider(ShelfHooks hooks) : hooks_(std::move(hooks)) {}
    ~ShelfProvider() override { *alive_ = false; } // a copy or check still running calls back into nothing

    const char* id() const override { return "shelf"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action& action) override;
    void onCardShown(const std::string& activityId) override; // links are checked when they're seen, never at start
    std::vector<DropZone> dropZones(const DragSummary& drag) override;
    void onDrop(const std::string& zoneId, const std::vector<DroppedItem>& items) override;

    const shelf::Items& items() const { return items_; }

    static constexpr const char* kItemsKey = "items";
    static constexpr const char* kLinkZone = "shelf.link";
    static constexpr const char* kCopyZone = "shelf.copy";

private:
    void changed(); // saves and shows the list
    void startCopy(std::uint64_t id, const DroppedItem& dropped);
    Host* host_ = nullptr;
    ShelfHooks hooks_;
    shelf::Items items_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace cup
