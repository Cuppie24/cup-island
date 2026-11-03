#pragma once
// Search (decisions 077, 079): a Search widget on Home, and its card: a live
// field and, as you type, apps first, then Settings pages and Control Panel
// items, then files. Enter starts the top one; a row starts its own. The
// card closes once something started (Host::closeCard).
//   - apps and Control Panel items come from Windows (SearchHooks::loadApps,
//     off the UI thread), kept and searched in memory, read again when the
//     card opens if older than launcherRefresh;
//   - Settings pages are our own list (launchables.h);
//   - files come from Cup Island's service, through the host
//     (Host::callService("index.query"), decision 078); an answer for an
//     older query is dropped.
// What was started is remembered per user and ranks higher (LaunchHistory).
// Pure, tested on Linux: Windows is behind SearchHooks.

#include "core/provider.h"
#include "indexer/protocol.h"
#include "providers/search/launchables.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cup {

struct SearchHooks {
    // Apps and Control Panel items, as Windows lists them; `done` on the UI thread.
    std::function<void(std::function<void(std::vector<Launchable>)> done)> loadApps;
    std::function<bool(const Launchable&)> launch;        // starts it; false if Windows couldn't
    std::function<bool(const std::string& path)> openFile; // opens it as Explorer would
    int windowsBuild = 0;
    bool russian = false; // Settings pages named in Russian (both names match)
};

namespace search_action {
inline constexpr const char* kTyped = "search.typed"; // the field's live edit (TextInput::changeAction)
inline constexpr const char* kGo = "search.go";       // Enter: the top result
inline constexpr const char* kOpen = "search.open.";  // + a result's index
} // namespace search_action

class SearchProvider : public Provider {
public:
    explicit SearchProvider(SearchHooks hooks) : hooks_(std::move(hooks)) {}
    ~SearchProvider() override { *alive_ = false; }

    static constexpr const char* kId = "search";
    static constexpr const char* kPanelId = "search";
    static constexpr const char* kHistoryKey = "launches";

    const char* id() const override { return kId; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action& action) override;
    void onCardShown(const std::string& activityId) override;

    // One result as the card lists it, in its order (for tests).
    struct Result {
        Launchable::Kind kind = Launchable::Kind::App;
        bool file = false;
        bool folder = false; // a file result that's a folder
        std::string name, caption, target;
    };
    std::vector<Result> results() const;
    const std::string& filesNote() const { return filesNote_; }

private:
    void refreshApps();
    void searchNow(); // ranks what's in memory and asks the service for files
    void publish();
    void open(std::size_t index);

    SearchHooks hooks_;
    Host* host_ = nullptr;
    std::vector<Launchable> apps_;     // from Windows, cleaned
    std::vector<Launchable> settings_; // our list, for this build
    Ms appsAt_{ -1 };                  // when apps_ was read; -1: never
    bool loading_ = false;
    LaunchHistory history_;
    std::string query_;
    std::vector<const Launchable*> shown_;   // apps, then Settings and Control Panel
    std::vector<indexer::ResultRow> files_;  // then files
    std::string filesNote_;                  // why there are no files ("isn't set up")
    std::uint64_t fileAsk_ = 0;              // the latest files request; older answers are dropped
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace cup
