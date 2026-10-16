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
    host_->publishPanel(shelf::shelfPanel(items_));
}

void ShelfProvider::stop()
{
    if (!host_) return;
    host_->removePanel(shelf::kPanelId);
    host_ = nullptr;
}

void ShelfProvider::changed()
{
    if (!host_) return;
    host_->save(kItemsKey, items_.save());
    host_->publishPanel(shelf::shelfPanel(items_));
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
    shelf::DropPlan plan = shelf::planDrop(summaryOf(dropped), shelf::limitsNow(items_.storeUsed()));
    const shelf::ZonePlan& zone = zoneId == kLinkZone ? plan.link : plan.copy;
    for (std::size_t i = 0; i < dropped.size() && i < zone.items.size(); ++i) {
        const DroppedItem& d = dropped[i];
        shelf::Item item{ .name = shownName(d), .what = d.what.kind };
        if (zone.items[i] == shelf::Handling::Link) {
            item.path = d.path;
            items_.add(item);
        } else if (zone.items[i] == shelf::Handling::Copy) {
            item.kind = shelf::Kind::Copy;
            item.state = shelf::State::Copying;
            item.source = d.path;
            item.bytes = d.what.bytes.value_or(0); // counts against the store while it copies
            startCopy(items_.add(item), d);
        }
    }
    changed();
    if (!items_.all().empty()) host_->reveal(shelf::kPanelId, act::kList, 0); // the newest, at the top
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
            if (*alive && items_.setProgress(id, fraction) && host_) host_->publishPanel(shelf::shelfPanel(items_));
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
    if (a.actionId == act::kClear) {
        if (!items_.all().empty()) host_->ask(shelf::clearQuestion(items_));
        return;
    }
    if (a.actionId == act::kClearConfirmed) {
        for (const shelf::Item& item : items_.all())
            if (item.kind == shelf::Kind::Copy && !item.path.empty() && hooks_.removeCopy) hooks_.removeCopy(item.path);
        items_.clear();
        changed();
        return;
    }
    if (auto id = idAfter(a.actionId, act::kOpen)) {
        const shelf::Item* item = items_.find(*id);
        if (item && item->state == shelf::State::Ready && hooks_.open) hooks_.open(item->path);
    } else if (auto id = idAfter(a.actionId, act::kRemove)) {
        const shelf::Item* item = items_.find(*id);
        if (!item) return;
        // A copy goes from the store; a link only from the list (its original stays).
        if (item->kind == shelf::Kind::Copy && !item->path.empty() && hooks_.removeCopy) hooks_.removeCopy(item->path);
        items_.remove(*id);
        changed();
    } else if (auto id = idAfter(a.actionId, act::kKeep)) {
        if (items_.keepAsLink(*id)) changed();
    }
}

void ShelfProvider::onCardShown(const std::string& activityId)
{
    if (activityId != shelf::kPanelId || !hooks_.checkLinks) return;
    std::vector<std::uint64_t> ids;
    std::vector<std::string> paths;
    for (const shelf::Item& item : items_.all()) {
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
