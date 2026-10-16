#pragma once
// The Shelf (decision 061): files, text and links dropped on the island,
// kept as links to the originals or as copies in its own store. It offers
// two zones for a drag, Link and Copy, each saying what it would do; a drop
// lands on its card, which stays open showing what came in. Copying, opening
// and checking files are the app's (ShelfHooks), so this stays pure and is
// tested on Linux.

#include "core/provider.h"
#include "providers/shelf/drop_plan.h"
#include "providers/shelf/shelf_content.h"
#include "providers/shelf/shelf_items.h"

#include <functional>
#include <memory>
#include <set>
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
    std::function<void(const std::string& path)> open;         // in its app, as Explorer would (a double-click)
    std::function<void(const std::string& path)> reveal;       // Explorer, with it selected
    // Renames a copy in its store folder, off the UI thread; `done` on the UI
    // thread with its new place, or why not. Never a file outside the store.
    std::function<void(const std::string& stored, const std::string& name,
                       std::function<void(const std::string& renamed, const std::string& problem)> done)> renameCopy;
    // Its picture, off the UI thread: Windows' thumbnail, or its type's icon;
    // `done` on the UI thread, nothing if there's none.
    std::function<void(const std::string& path, std::function<void(std::shared_ptr<const ImageData>)> done)> thumbnail;
    // Windows' file picker (the add card, decision 064): `done` on the UI thread with what was picked, sizes read; nothing if cancelled.
    std::function<void(std::function<void(std::vector<DroppedItem>)> done)> pickFiles;
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
    void onCardShown(const std::string& activityId) override; // links checked and pictures read when it's seen, never at start
    std::vector<DropZone> dropZones(const DragSummary& drag) override;
    void onDrop(const std::string& zoneId, const std::vector<DroppedItem>& items) override;
    void onSelectionChanged(const std::string& activityId, const std::string& list, const std::vector<std::string>& ids) override;
    // The picked things' files, to drag out (decision 065): a copy's in the store, a link's original. Only what's there.
    std::vector<std::string> dragOut(const std::string& activityId, const std::string& list, const std::vector<std::string>& ids) override;
    // The right-click menu (decision 066), like Explorer's: one thing's actions, Delete N items for several, Select all.
    std::vector<MenuItem> contextMenu(const std::string& activityId, const std::string& list, const std::vector<std::string>& ids) override;

    const shelf::Items& items() const { return items_; }
    shelf::View view() const { return view_; }

    static constexpr const char* kItemsKey = "items";
    static constexpr const char* kViewKey = "view"; // "cards" (the default) or "list"
    static constexpr const char* kLinkZone = "shelf.link";
    static constexpr const char* kCopyZone = "shelf.copy";

private:
    void changed(); // saves and shows the list
    void show();    // shows it, nothing to save (progress, a picture)
    void startCopy(std::uint64_t id, const DroppedItem& dropped);
    void rename(std::uint64_t id, const std::string& name);
    void remove(std::uint64_t id); // off the shelf: a copy deleted from the store, a link only from the list
    std::size_t add(const std::string& zoneId, const std::vector<DroppedItem>& items); // as that zone plans; returns how many became links
    void readThumbnails(); // for ready things without a picture, once each
    Host* host_ = nullptr;
    ShelfHooks hooks_;
    shelf::Items items_;
    shelf::View view_ = shelf::View::Cards;
    shelf::Thumbnails thumbnails_;     // by item id; in memory only
    std::set<std::uint64_t> asked_;    // pictures asked for (an answer of none isn't asked again)
    bool shown_ = false;               // its card has been seen: pictures are read from then on
    std::vector<std::uint64_t> picked_; // what's selected on its card (decision 064)
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace cup
