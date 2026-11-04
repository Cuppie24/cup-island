#include "providers/search/search_provider.h"

#include "core/tokens.h"
#include "indexer/query.h"
#include "providers/search/result_rows.h"

#include <algorithm>
#include <cstdlib>

namespace cup {
namespace {

namespace A = search_action;

const char* captionOf(Launchable::Kind kind)
{
    switch (kind) {
    case Launchable::Kind::App: return "App";
    case Launchable::Kind::ControlPanel: return "Control Panel";
    case Launchable::Kind::Setting: return "Settings";
    }
    return "";
}

Glyph glyphOf(Launchable::Kind kind) { return kind == Launchable::Kind::App ? Glyph::OpenApp : Glyph::Settings; }

// A result (decision 079): its icon is the row's button, so the whole row
// opens it; its name, the typed words in it brighter; under it, where it is
// (a path loses its start when too long, so the end tells two apart). A
// file's folder button shows under the pointer.
Row resultRow(Glyph glyph, std::shared_ptr<const ImageData> picture, const SearchProvider::Result& r,
              const std::string& query, const std::string& home, std::size_t index)
{
    Row row{ .items = { Button{ .glyph = glyph, .actionId = A::kOpen + std::to_string(index), .compact = true, .picture = std::move(picture) },
                        Text{ .value = r.name, .detail = r.file ? shortPath(r.caption, home) : r.caption, .bright = matchedParts(r.name, query) } },
             .clickAnywhere = true,
             .list = "results",
             .listItem = true };
    if (r.file)
        row.trailing.push_back(Button{ .glyph = Glyph::Folder, .actionId = A::kReveal + std::to_string(index), .compact = true,
                                       .revealOnHover = true, .secondary = true });
    return row;
}

Row fieldRow()
{
    return row({ TextInput{ .actionId = A::kGo, .placeholder = "Search apps, settings and files", .large = true, .changeAction = A::kTyped } });
}

} // namespace

void SearchProvider::start(Host& host)
{
    host_ = &host;
    settings_ = settingsPages(hooks_.windowsBuild, hooks_.russian);
    history_ = LaunchHistory::load(host.load(kHistoryKey));
    searchNow();
    refreshApps();
}

void SearchProvider::stop()
{
    host_->removePanel(kPanelId);
}

void SearchProvider::refreshApps()
{
    if (loading_ || !hooks_.loadApps) return;
    loading_ = true;
    hooks_.loadApps([this, alive = alive_](std::vector<Launchable> apps) {
        if (!*alive) return;
        loading_ = false;
        apps_ = cleanedApps(std::move(apps));
        appsAt_ = host_->now();
        searchNow();
    });
}

void SearchProvider::onCardShown(const std::string& activityId)
{
    if (activityId != kPanelId) return;
    if (appsAt_ < Ms{ 0 } || host_->now() - appsAt_ >= tokens().launcherRefresh) refreshApps();
}

void SearchProvider::onCardClosed(const std::string& activityId)
{
    // It opens empty (decision 079): the field's text goes with the card
    // (card_overlays), and so do the results.
    if (activityId != kPanelId || query_.empty()) return;
    query_.clear();
    searchNow();
}

void SearchProvider::onAction(const Action& action)
{
    if (action.actionId == A::kTyped) {
        query_ = action.text;
        searchNow();
        host_->reveal(kPanelId, "results", 0); // new words: the list starts at the top again, not where it was scrolled
    } else if (action.actionId == A::kGo) {
        if (action.text != query_) { // Enter before the last edit's answer: search what was typed
            query_ = action.text;
            searchNow();
        }
        open(0);
    } else if (action.actionId.rfind(A::kOpen, 0) == 0) {
        open(static_cast<std::size_t>(std::strtoul(action.actionId.c_str() + std::string_view(A::kOpen).size(), nullptr, 10)));
    } else if (action.actionId.rfind(A::kReveal, 0) == 0) {
        reveal(static_cast<std::size_t>(std::strtoul(action.actionId.c_str() + std::string_view(A::kReveal).size(), nullptr, 10)));
    }
}

void SearchProvider::searchNow()
{
    // Apps and the system's pages from memory, at once; files from the service.
    std::vector<Launchable> all;
    all.reserve(apps_.size() + settings_.size());
    std::vector<const Launchable*> sources;
    for (const Launchable& a : apps_) sources.push_back(&a);
    for (const Launchable& s : settings_) sources.push_back(&s);
    for (const Launchable* l : sources) all.push_back(*l);
    Ms wall = host_->wallClock();
    std::vector<const Launchable*> apps, system;
    for (std::size_t i : rankLaunchables(all, query_, history_, wall, all.size())) {
        const Launchable* l = sources[i];
        auto& group = l->kind == Launchable::Kind::App ? apps : system;
        std::size_t cap = l->kind == Launchable::Kind::App ? tokens().searchAppRows : tokens().searchSettingRows;
        if (group.size() < cap) group.push_back(l);
    }
    shown_ = apps;
    shown_.insert(shown_.end(), system.begin(), system.end());

    std::uint64_t ask = ++fileAsk_;
    bool typed = !indexer::parseQuery(query_).empty();
    if (!typed) { // nothing typed: the field alone (decision 079)
        shown_.clear();
        files_.clear();
        filesNote_.clear();
        publish();
        return;
    }
    publish(); // apps show now; files follow when the service answers
    host_->callService("index.query", indexer::queryParams(query_, tokens().searchFileRows),
                       [this, ask, alive = alive_](ServiceReply reply) {
                           if (!*alive || ask != fileAsk_) return; // a newer query's answer is what counts
                           files_.clear();
                           filesNote_.clear();
                           using P = ServiceReply::Problem;
                           if (reply.problem == P::None) {
                               auto parsed = indexer::parseResponse(reply.body);
                               if (parsed && parsed->rows) files_ = *parsed->rows;
                           } else if (reply.problem == P::NotInstalled) {
                               filesNote_ = "File search isn't set up";
                           } else if (reply.problem == P::NotRunning) {
                               filesNote_ = "File search is starting";
                           }
                           publish();
                       });
}

std::vector<SearchProvider::Result> SearchProvider::results() const
{
    std::vector<Result> out;
    for (const Launchable* l : shown_) out.push_back({ l->kind, false, false, l->name, captionOf(l->kind), l->target });
    for (const indexer::ResultRow& f : files_) out.push_back({ Launchable::Kind::App, true, f.folder, f.name, folderOf(f.path), f.path });
    return out;
}

void SearchProvider::open(std::size_t index)
{
    std::vector<Result> all = results();
    if (index >= all.size()) return;
    const Result& r = all[index];
    bool started = false;
    if (r.file) {
        started = hooks_.openFile && hooks_.openFile(r.target);
    } else {
        started = hooks_.launch && hooks_.launch(*shown_[index]);
        if (started) {
            history_.record(r.target, host_->wallClock());
            host_->save(kHistoryKey, history_.save());
        }
    }
    if (started) host_->closeCard(kPanelId); // out of the way of what it started
}

void SearchProvider::reveal(std::size_t index)
{
    std::vector<Result> all = results();
    if (index >= all.size() || !all[index].file) return;
    if (hooks_.reveal && hooks_.reveal(all[index].target)) host_->closeCard(kPanelId);
}

std::shared_ptr<const ImageData> SearchProvider::iconFor(const std::string& target)
{
    if (auto it = icons_.find(target); it != icons_.end()) return it->second;
    if (!hooks_.loadIcon) return nullptr;
    if (icons_.size() >= tokens().searchIconCache) icons_.clear(); // rarely: read again as they're shown
    icons_[target] = nullptr; // asked: not again while it comes
    hooks_.loadIcon(target, [this, target, alive = alive_](std::shared_ptr<const ImageData> icon) {
        if (!*alive || !icon) return;
        icons_[target] = std::move(icon);
        publish();
    });
    return nullptr;
}

void SearchProvider::publish()
{
    Content c;
    c.label = "Search";
    c.tileLeading = { row({ Icon{ Glyph::Search } }, Align::Center) };
    c.tile = { row({ Text{ "Search", TextStyle::Title } }) };
    // Wide, with a big field (CardLayout::Search, decision 079), which has
    // the keys at once and closes with one Esc (Content::fieldFirst). A near
    // miss between rows doesn't close it. As tall as its rows: the field
    // alone until something's typed, then up to searchVisibleRows results.
    c.card = CardLayout::Search;
    c.fieldFirst = A::kGo;
    c.keepOpen = true;
    c.expandedCenter.push_back(fieldRow());
    std::vector<Result> all = results();
    bool typed = !indexer::parseQuery(query_).empty();
    std::string note = !filesNote_.empty() ? filesNote_ : typed && all.empty() ? "Nothing found" : "";
    if (typed && !note.empty()) c.expandedCenter.push_back(row({ Text{ .value = note, .style = TextStyle::Caption, .centred = true } }));
    for (std::size_t i = 0; i < all.size(); ++i) {
        const Result& r = all[i];
        Glyph glyph = r.file ? (r.folder ? Glyph::Folder : Glyph::File) : glyphOf(r.kind);
        c.expandedCenter.push_back(resultRow(glyph, iconFor(r.target), r, query_, hooks_.home, i));
        if (i == 0) c.expandedCenter.back().visibleRows = tokens().searchVisibleRows; // the list's window, from its first row
    }
    host_->publishPanel({ .id = kPanelId, .content = std::move(c) });
}

} // namespace cup
