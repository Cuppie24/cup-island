#include "providers/shelf/shelf_provider.h"

#include "core/tokens.h"
#include "providers/shelf/safe_names.h"
#include "providers/shelf/shelf_content.h"

#include <optional>

namespace cup {
namespace {

namespace act = shelf::action;

// The item id after an action's prefix, if the action has it.
std::optional<std::uint64_t> idAfter(const std::string& actionId, const char* prefix)
{
    std::string p = prefix;
    if (!actionId.starts_with(p)) return std::nullopt;
    try {
        return std::stoull(actionId.substr(p.size()));
    } catch (...) {
        return std::nullopt;
    }
}

// The drop as the plan sees it: the same rules decide what the zones said and what happens.
DragSummary summaryOf(const std::vector<DroppedItem>& items)
{
    DragSummary drag{ .total = items.size(), .sizesKnown = true };
    for (const DroppedItem& d : items) {
        drag.items.push_back(d.what);
        if (d.what.kind == DragKind::File && !d.what.bytes) drag.sizesKnown = false;
    }
    return drag;
}

std::string shownName(const DroppedItem& d)
{
    if (d.what.kind == DragKind::Text) return shelf::textFileName(d.text);
    if (d.what.kind == DragKind::Url) return shelf::urlFileName(d.text, d.title);
    return d.what.name;
}

} // namespace

void ShelfProvider::start(Host& host)
{
    host_ = &host;
    items_ = shelf::Items::load(host_->load(kItemsKey));
    view_ = host_->load(kViewKey) == "list" ? shelf::View::List : shelf::View::Cards;
    show();
}

void ShelfProvider::stop()
{
    if (!host_) return;
    host_->removePanel(shelf::kPanelId);
    host_ = nullptr;
}

void ShelfProvider::show()
{
    if (host_) host_->publishPanel(shelf::shelfPanel(items_, view_, thumbnails_, picked_.size()));
}

void ShelfProvider::changed()
{
    if (!host_) return;
    host_->save(kItemsKey, items_.save());
    readThumbnails();
    show();
}

void ShelfProvider::readThumbnails()
{
    if (!hooks_.thumbnail || !shown_) return;
    for (const shelf::Entry& item : items_.all()) {
        if (item.state != shelf::State::Ready || item.path.empty() || asked_.count(item.id)) continue;
        asked_.insert(item.id);
        auto alive = alive_;
        std::uint64_t id = item.id;
        hooks_.thumbnail(item.path, [this, alive, id](std::shared_ptr<const ImageData> picture) {
            if (!*alive || !picture || !items_.find(id)) return;
            thumbnails_[id] = std::move(picture);
            show();
        });
    }
}

// Decision 063: a link is only relabelled on the shelf (the user's file keeps
// its name); a copy's file is renamed in the store.
void ShelfProvider::rename(std::uint64_t id, const std::string& typed)
{
    const shelf::Entry* item = items_.find(id);
    std::string name = typed;
    name.erase(0, name.find_first_not_of(" \t"));
    name.erase(name.find_last_not_of(" \t") + 1);
    if (!item || name.empty()) return;
    if (item->kind == shelf::Kind::Link) {
        if (items_.relabel(id, name)) changed();
        return;
    }
    std::string safe = shelf::safeFileName(name);
    // ".partial" is how an unfinished copy is named: the next start would delete it.
    if (item->state != shelf::State::Ready || safe == item->name || safe.ends_with(".partial") || !hooks_.renameCopy) return;
    auto alive = alive_;
    hooks_.renameCopy(item->path, safe, [this, alive, id, safe](const std::string& renamed, const std::string& problem) {
        if (*alive && problem.empty() && items_.renamed(id, renamed, safe)) changed();
    });
}

std::vector<DropZone> ShelfProvider::dropZones(const DragSummary& drag)
{
    if (drag.items.empty()) return {};
    shelf::DropPlan plan = shelf::planDrop(drag, shelf::limitsNow(items_.storeUsed()));
    auto zone = [](const char* id, Glyph glyph, const shelf::ZonePlan& p, DropEffect effect) {
        return DropZone{ .id = id, .cardId = shelf::kPanelId, .glyph = glyph, .label = p.label, .reason = p.reason,
                         .enabled = p.enabled, .effect = effect };
    };
    return { zone(kLinkZone, Glyph::Link, plan.link, DropEffect::Link), zone(kCopyZone, Glyph::Copy, plan.copy, DropEffect::Copy) };
}

void ShelfProvider::onDrop(const std::string& zoneId, const std::vector<DroppedItem>& dropped)
{
    if (!host_ || (zoneId != kLinkZone && zoneId != kCopyZone)) return;
    add(zoneId, dropped);
}

std::size_t ShelfProvider::add(const std::string& zoneId, const std::vector<DroppedItem>& dropped)
{
    std::size_t linked = 0;
    shelf::DropPlan plan = shelf::planDrop(summaryOf(dropped), shelf::limitsNow(items_.storeUsed()));
    const shelf::ZonePlan& zone = zoneId == kLinkZone ? plan.link : plan.copy;
    for (std::size_t i = 0; i < dropped.size() && i < zone.items.size(); ++i) {
        const DroppedItem& d = dropped[i];
        shelf::Entry item{ .name = shownName(d), .what = d.what.kind };
        if (zone.items[i] == shelf::Handling::Link) {
            item.path = d.path;
            item.bytes = d.what.bytes.value_or(0); // shown in the shelf's total, never counted against its store
            items_.add(item);
            ++linked;
        } else if (zone.items[i] == shelf::Handling::Copy) {
            item.kind = shelf::Kind::Copy;
            item.state = shelf::State::Copying;
            item.source = d.path;
            item.bytes = d.what.bytes.value_or(0); // counts against the store while it copies
            startCopy(items_.add(item), d);
        }
    }
    changed();
    // The newest, first: in view, with the keyboard's focus (decision 063).
    if (!items_.all().empty()) host_->reveal(shelf::kPanelId, act::kList, 0, true);
    return linked;
}

void ShelfProvider::remove(std::uint64_t id)
{
    const shelf::Entry* item = items_.find(id);
    if (!item) return;
    // A copy goes from the store; a link only from the list (its original stays).
    if (item->kind == shelf::Kind::Copy && !item->path.empty() && hooks_.removeCopy) hooks_.removeCopy(item->path);
    items_.remove(id);
    thumbnails_.erase(id);
    std::erase(picked_, id);
}

std::vector<std::string> ShelfProvider::dragOut(const std::string& activityId, const std::string& list, const std::vector<std::string>& ids)
{
    std::vector<std::string> paths;
    if (activityId != shelf::kPanelId || list != act::kList) return paths;
    for (const std::string& row : ids)
        if (auto id = shelf::itemOfRow(row))
            if (const shelf::Entry* item = items_.find(*id); item && item->state == shelf::State::Ready && !item->path.empty())
                paths.push_back(item->path);
    return paths;
}

std::vector<MenuItem> ShelfProvider::contextMenu(const std::string& activityId, const std::string& list, const std::vector<std::string>& ids)
{
    std::vector<MenuItem> menu;
    if (activityId != shelf::kPanelId || list != act::kList) return menu;
    std::vector<const shelf::Entry*> picked;
    for (const std::string& row : ids)
        if (auto id = shelf::itemOfRow(row))
            if (const shelf::Entry* item = items_.find(*id)) picked.push_back(item);
    if (picked.size() == 1) {
        const shelf::Entry& item = *picked[0];
        std::string id = std::to_string(item.id);
        bool ready = item.state == shelf::State::Ready && !item.path.empty();
        if (ready) {
            menu.push_back({ .glyph = Glyph::OpenApp, .label = "Open", .actionId = act::kOpen + id });
            menu.push_back({ .glyph = Glyph::Reveal, .label = "Show in folder", .actionId = act::kReveal + id });
        }
        if (item.state != shelf::State::Copying)
            menu.push_back({ .glyph = Glyph::Edit, .label = "Rename", .actionId = act::kRename + id, .editsText = true });
        if (item.state == shelf::State::Failed && !item.source.empty())
            menu.push_back({ .glyph = Glyph::Link, .label = "Keep as link", .actionId = act::kKeep + id });
        menu.push_back({ .glyph = Glyph::Delete, .label = "Remove", .actionId = act::kRemove + id, .destructive = true });
    } else if (picked.size() > 1) {
        menu.push_back({ .glyph = Glyph::Delete, .label = "Delete " + std::to_string(picked.size()) + " items", .actionId = act::kDeleteSelected,
                         .destructive = true });
    }
    if (!items_.all().empty()) menu.push_back({ .glyph = Glyph::Checked, .label = "Select all", .actionId = kMenuSelectAll });
    return menu;
}

void ShelfProvider::onSelectionChanged(const std::string& activityId, const std::string& list, const std::vector<std::string>& ids)
{
    if (activityId != shelf::kPanelId || list != act::kList) return;
    picked_.clear();
    for (const std::string& row : ids)
        if (auto id = shelf::itemOfRow(row)) picked_.push_back(*id);
    show(); // its foot says Delete N items
}

void ShelfProvider::startCopy(std::uint64_t id, const DroppedItem& d)
{
    if (!hooks_.copy) {
        items_.failed(id, "Copying isn't available");
        return;
    }
    CopyJob job{ .id = id, .name = shelf::safeFileName(shownName(d)), .maxBytes = shelf::limitsNow(0).copyMaxBytes };
    if (d.what.kind == DragKind::Text) job.contents = shelf::textFileBody(d.text);
    else if (d.what.kind == DragKind::Url) job.contents = shelf::urlFileBody(d.text);
    else job.source = d.path;
    auto alive = alive_;
    hooks_.copy(job,
        [this, alive, id](float fraction) {
            if (*alive && items_.setProgress(id, fraction)) show();
        },
        [this, alive, id](const std::string& stored, std::uint64_t bytes, const std::string& problem) {
            if (!*alive) return;
            bool known = problem.empty() ? items_.copied(id, stored, bytes) : items_.failed(id, problem);
            // Removed while it copied: the copy has nowhere to go.
            if (!known && problem.empty() && hooks_.removeCopy) hooks_.removeCopy(stored);
            if (known) changed();
        });
}

void ShelfProvider::onAction(const Action& a)
{
    if (!host_ || a.activityId != shelf::kPanelId) return;
    if (a.actionId == act::kView) {
        view_ = view_ == shelf::View::Cards ? shelf::View::List : shelf::View::Cards;
        host_->save(kViewKey, view_ == shelf::View::List ? "list" : "cards");
        show();
        return;
    }
    if (a.actionId == act::kAdd && hooks_.pickFiles) { // Windows' picker: kept as copies, as the Copy zone would (decision 064)
        auto alive = alive_;
        hooks_.pickFiles([this, alive](std::vector<DroppedItem> picked) {
            if (!*alive || !host_ || picked.empty()) return;
            std::size_t linked = add(kCopyZone, picked);
            if (linked == 0) return;
            // Some couldn't be copied (too large, a folder): say so, in a toast.
            std::string what = linked == 1 ? "1 file was added as a link" : std::to_string(linked) + " files were added as links";
            host_->postAlert({ .activityId = shelf::kPanelId, .content = { .label = what,
                                 .alertLeading = { row({ Icon{ Glyph::Link } }) },
                                 .alertCenter = { row({ Text{ what, TextStyle::Title } }), row({ Text{ "Too large to copy, or a folder", TextStyle::Caption } }) } } });
        });
        return;
    }
    if (a.actionId == act::kDeleteSelected) {
        if (!picked_.empty()) host_->ask(shelf::deleteQuestion(items_, picked_));
        return;
    }
    if (a.actionId == act::kDeleteSelectedConfirmed) {
        for (std::uint64_t id : std::vector<std::uint64_t>(picked_)) remove(id);
        changed();
        return;
    }
    if (a.actionId == act::kClear) {
        if (!items_.all().empty()) host_->ask(shelf::clearQuestion(items_));
        return;
    }
    if (a.actionId == act::kClearConfirmed) {
        for (const shelf::Entry& item : items_.all())
            if (item.kind == shelf::Kind::Copy && !item.path.empty() && hooks_.removeCopy) hooks_.removeCopy(item.path);
        items_.clear();
        thumbnails_.clear();
        picked_.clear();
        changed();
        return;
    }
    if (auto id = idAfter(a.actionId, act::kOpen)) {
        const shelf::Entry* item = items_.find(*id);
        if (item && item->state == shelf::State::Ready && hooks_.open) hooks_.open(item->path);
    } else if (auto id = idAfter(a.actionId, act::kReveal)) {
        const shelf::Entry* item = items_.find(*id);
        if (item && item->state == shelf::State::Ready && hooks_.reveal) hooks_.reveal(item->path);
    } else if (auto id = idAfter(a.actionId, act::kRename)) {
        rename(*id, a.text);
    } else if (auto id = idAfter(a.actionId, act::kRemoveConfirmed)) {
        remove(*id);
        changed();
    } else if (auto id = idAfter(a.actionId, act::kRemove)) {
        if (const shelf::Entry* item = items_.find(*id)) host_->ask(shelf::removeQuestion(*item)); // asks first (decision 064)
    } else if (auto id = idAfter(a.actionId, act::kKeep)) {
        if (items_.keepAsLink(*id)) changed();
    }
}

void ShelfProvider::onCardShown(const std::string& activityId)
{
    if (activityId != shelf::kPanelId) return;
    shown_ = true;
    readThumbnails();
    if (!hooks_.checkLinks) return;
    std::vector<std::uint64_t> ids;
    std::vector<std::string> paths;
    for (const shelf::Entry& item : items_.all()) {
        if (item.kind != shelf::Kind::Link || (item.state != shelf::State::Ready && item.state != shelf::State::Missing)) continue;
        ids.push_back(item.id);
        paths.push_back(item.path);
    }
    if (paths.empty()) return;
    auto alive = alive_;
    hooks_.checkLinks(paths, [this, alive, ids](std::vector<bool> present) {
        if (!*alive) return;
        bool any = false;
        for (std::size_t i = 0; i < ids.size() && i < present.size(); ++i) any = items_.setMissing(ids[i], !present[i]) || any;
        if (any) changed();
    });
}

} // namespace cup
