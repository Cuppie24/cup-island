#pragma once
// What search can start (decision 077): apps and Control Panel items, read
// from Windows' shell folders by the app, and Settings pages, from our own
// list, since Windows publishes none. Which to keep, how they rank and what
// was launched before. Pure, tested on Linux.

#include "core/island.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cup {

struct Launchable {
    enum class Kind { App, ControlPanel, Setting };
    Kind kind = Kind::App;
    std::string name;   // as Windows shows it, in its language
    std::string target; // App, ControlPanel: the shell item's parsing name; Setting: its ms-settings: link
    std::vector<std::string> words; // more to match (a Settings page's "wifi", "вайфай"), lower case
    std::shared_ptr<const ImageData> icon; // App and ControlPanel, if Windows had one
    bool operator==(const Launchable&) const = default;
};

// Clean-up (decision 077): drops what nobody launches from a search, by
// name ("Uninstall X", "Удалить X") or target (help files, web links,
// documents), and duplicates of one target. Keeps the order otherwise.
std::vector<Launchable> cleanedApps(std::vector<Launchable> apps);

// Settings pages that exist on this Windows build (decision 015: 10 and 11
// differ; an unknown link only opens Settings' home). Named in Russian when
// `russian`, in English otherwise; both names match either way.
std::vector<Launchable> settingsPages(int windowsBuild, bool russian);

// What was launched from search, per user (Host::save), by target: how
// often, and when last (wall time). Files opened from search keep one of
// their own (recent_files, decision 082).
struct LaunchHistory {
    struct Use {
        int count = 0;
        Ms last{ 0 };
        bool operator==(const Use&) const = default;
    };
    std::map<std::string, Use> uses;

    void record(const std::string& target, Ms now); // keeps the newest kMaxHistory
    std::string save() const;                       // "<count> <ms> <target>", a line each
    static LaunchHistory load(std::string_view text);
    // How much past use counts now: launches in the last month, the newer the more.
    double weight(const std::string& target, Ms now) const;
};
constexpr std::size_t kMaxHistory = 300;

// How well a query's words match one item (0: not every word matches):
// its name as file search scores it (indexer::matchScore), its initials
// ("vsc" for Visual Studio Code, "вк" for ВКонтакте) as a word start, or
// one of its words.
int launchScore(const Launchable& item, std::string_view query);

// The best `limit` of `items` for `query`, best first: score, then past use
// (decision 077), then the shorter name. Indices into `items`. An empty
// query lists the most used, if any were used; one with only paths or
// filters ("d:/projects", "ext:pdf") lists nothing: those are for files.
std::vector<std::size_t> rankLaunchables(const std::vector<Launchable>& items, std::string_view query,
                                          const LaunchHistory& history, Ms now, std::size_t limit);

} // namespace cup
