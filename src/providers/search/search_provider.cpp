#include "providers/search/search_provider.h"

#include "core/tokens.h"
#include "indexer/query.h"

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

// The folder a file is in, for its row's caption.
std::string folderOf(const std::string& path)
{
    std::size_t slash = path.find_last_of('\\');
    return slash == std::string::npos ? path : path.substr(0, slash);
}

// A result: its icon, its name, what it is, and the chevron that says the
// whole row is a button (as Settings' rows, decision 030).
// The folder gives way first when the row is short of room (Text::yields):
// the name stays readable and the path takes the "…".
Row resultRow(Glyph glyph, const std::string& name, const std::string& caption, std::size_t index)
{
    return Row{ .items = { Icon{ glyph }, Text{ name },
                           Text{ .value = caption, .style = TextStyle::Caption, .alignEnd = true, .yields = true },
                           Button{ .glyph = Glyph::ChevronRight, .actionId = A::kOpen + std::to_string(index), .compact = true } },
                .clickAnywhere = true,
                .list = "results",
                .listItem = true };
}

Row fieldRow()
{
    return row({ TextInput{ .actionId = A::kGo, .placeholder = "Search apps, settings and files", .changeAction = A::kTyped } });
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
    if (!typed) {
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

void SearchProvider::publish()
{
    Content c;
    c.label = "Search";
    c.tileLeading = { row({ Icon{ Glyph::Search } }, Align::Center) };
    c.tile = { row({ Text{ "Search", TextStyle::Title } }) };
    // A page of rows edge to edge, like Settings (decision 030), whose field
    // has the keys at once and closes with one Esc (Content::fieldFirst). A
    // near miss between rows doesn't close it.
    c.card = CardLayout::List;
    c.fieldFirst = A::kGo;
    c.keepOpen = true;
    // One height whatever the results: sized by the field and a full window of
    // rows, so the card doesn't jump as you type. The results start at the top
    // and fill what's under the field (a fill list, Row::visibleRows < 0).
    c.sizedLike.push_back(fieldRow());
    auto rows = static_cast<std::size_t>(tokens().searchVisibleRows + 0.999f);
    for (std::size_t i = 0; i < rows; ++i) c.sizedLike.push_back(resultRow(Glyph::Search, "x", "x", i));
    if (rows > 0) c.sizedLike[1].visibleRows = tokens().searchVisibleRows; // the window, from the list's first row
    c.pageAtTop = true;

    c.expandedCenter.push_back(fieldRow());
    std::vector<Result> all = results();
    bool typed = !indexer::parseQuery(query_).empty();
    // Said under the field, above the list: the list must be the card's last rows to fill it.
    std::string note = !filesNote_.empty()     ? filesNote_
                       : typed && all.empty()  ? "Nothing found"
                       : !typed && all.empty() ? "Type to find apps, settings and files"
                                               : "";
    if (!note.empty()) c.expandedCenter.push_back(row({ Text{ .value = note, .style = TextStyle::Caption, .centred = true } }));
    for (std::size_t i = 0; i < all.size(); ++i) {
        const Result& r = all[i];
        Glyph glyph = r.file ? (r.folder ? Glyph::Folder : Glyph::File) : glyphOf(r.kind);
        c.expandedCenter.push_back(resultRow(glyph, r.name, r.caption, i));
        if (i == 0) c.expandedCenter.back().visibleRows = -1; // fills the card's height under the field
    }
    host_->publishPanel({ .id = kPanelId, .content = std::move(c) });
}

} // namespace cup
