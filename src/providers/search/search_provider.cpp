#include "providers/search/search_provider.h"

#include "core/tokens.h"
#include "indexer/query.h"
#include "providers/search/recent_files.h"
#include "providers/search/result_menu.h"
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
// (a path loses its start when too long, so the end tells two apart). Its
// menu has the rest (decision 086): the host ends the row in a › for it.
Row resultRow(Glyph glyph, std::shared_ptr<const ImageData> picture, const SearchProvider::Result& r,
              const std::string& query, const std::string& home, std::size_t index)
{
    Row row{ .items = { Button{ .glyph = glyph, .actionId = A::kOpen + std::to_string(index), .picture = std::move(picture) },
                        Text{ .value = r.name, .style = TextStyle::Result, .detail = r.file ? shortPath(r.caption, home) : r.caption, .bright = matchedParts(r.name, query) } },
             .clickAnywhere = true,
             .list = "results",
             .listItem = true,
             .menu = resultMenu(r, index) };
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
    opened_ = LaunchHistory::load(host.load(kOpenedKey));
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
    } else {
        // The rest of a result's menu (decision 086): "search.<verb>.<index>".
        for (const char* verb : { A::kAdmin, A::kTerminal, A::kCopy, A::kCopyPath, A::kForget, A::kAppSettings, A::kUninstall, A::kProperties })
            if (action.actionId.rfind(verb, 0) == 0) {
                menuAction(verb, std::strtoul(action.actionId.c_str() + std::string_view(verb).size(), nullptr, 10));
                return;
            }
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
                           std::vector<indexer::ResultRow> rows;
                           filesNote_.clear();
                           using P = ServiceReply::Problem;
                           if (reply.problem == P::None) {
                               auto parsed = indexer::parseResponse(reply.body);
                               if (parsed && parsed->rows) rows = *parsed->rows;
                           } else if (reply.problem == P::NotInstalled) {
                               filesNote_ = "File search isn't set up";
                           } else if (reply.problem == P::NotRunning) {
                               filesNote_ = "File search is starting";
                           }
                           setFiles(std::move(rows)); // without the service, recent files still show
                           publish();
                       });
}

void SearchProvider::setFiles(std::vector<indexer::ResultRow> rows)
{
    std::vector<std::string> gone;
    files_ = withRecent(std::move(rows), opened_, query_, host_->wallClock(), tokens().searchFileRows, hooks_.exists, gone);
    if (gone.empty()) return;
    for (const std::string& key : gone) opened_.uses.erase(key); // deleted or moved: not offered again
    host_->save(kOpenedKey, opened_.save());
}

std::vector<SearchProvider::Result> SearchProvider::results() const
{
    std::vector<Result> out;
    for (const Launchable* l : shown_)
        out.push_back({ l->kind, false, false, l->name, captionOf(l->kind), l->target, l->package, l->location, history_.uses.count(l->target) > 0 });
    for (const indexer::ResultRow& f : files_) {
        bool recent = opened_.uses.count(recentKey({ .name = f.name, .path = f.path, .folder = f.folder })) > 0;
        out.push_back({ Launchable::Kind::App, true, f.folder, f.name, folderOf(f.path), f.path, {}, {}, recent });
    }
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
        if (started) { // ranks higher next time (decision 082)
            opened_.record(recentKey({ .name = r.name, .path = r.target, .folder = r.folder }), host_->wallClock());
            host_->save(kOpenedKey, opened_.save());
        }
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
    std::string path = index < all.size() ? pathOf(all[index]) : std::string();
    if (path.empty()) return; // a Store app, a Settings page: nowhere to show
    if (hooks_.reveal && hooks_.reveal(path)) host_->closeCard(kPanelId);
}

void SearchProvider::menuAction(const std::string& verb, std::size_t index)
{
    std::vector<Result> all = results();
    if (index >= all.size()) return;
    const Result& r = all[index];
    std::string path = pathOf(r);
    auto call = [](const auto& hook, const std::string& arg) { return hook && !arg.empty() && hook(arg); };
    // Copying and forgetting leave the card open: next is pasting, or another pick.
    if (verb == A::kCopy) {
        if (r.file) call(hooks_.copyFile, r.target);
        return;
    }
    if (verb == A::kCopyPath) {
        call(hooks_.copyText, !r.file && r.kind == Launchable::Kind::Setting ? r.target : path); // a Settings page's path is its link
        return;
    }
    if (verb == A::kForget) {
        if (forget(r)) searchNow(); // its rank changes: what's shown is worked out again
        return;
    }
    bool started = false;
    if (verb == A::kAdmin && elevates(r)) started = call(hooks_.runAsAdmin, r.target);
    else if (verb == A::kTerminal) started = call(hooks_.openTerminal, r.folder ? path : folderOf(path));
    else if (verb == A::kProperties) started = call(hooks_.properties, path);
    else if (verb == A::kUninstall || verb == A::kAppSettings) {
        // Installed apps in Settings; a Store app's own page there, by its package.
        std::string link = verb == A::kAppSettings && !r.package.empty() ? "ms-settings:appsfeatures-app?" + r.package : "ms-settings:appsfeatures";
        started = hooks_.launch && hooks_.launch(Launchable{ .kind = Launchable::Kind::Setting, .target = link });
    }
    if (started) host_->closeCard(kPanelId); // out of the way of the window it opened
}

bool SearchProvider::forget(const Result& r)
{
    LaunchHistory& from = r.file ? opened_ : history_;
    std::string key = r.file ? recentKey({ .name = r.name, .path = r.target, .folder = r.folder }) : r.target;
    if (from.uses.erase(key) == 0) return false;
    host_->save(r.file ? kOpenedKey : kHistoryKey, from.save());
    return true;
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
    c.hidesTabs = true; // alone on the island while you look (decision 079)
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
