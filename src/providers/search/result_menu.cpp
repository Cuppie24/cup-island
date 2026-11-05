#include "providers/search/result_menu.h"

#include "indexer/query.h"
#include "providers/search/result_rows.h"

namespace cup {
namespace {

namespace A = search_action;

// A file that's a program or a script: run as administrator like an app.
bool runnableFile(const std::string& path)
{
    std::size_t dot = path.find_last_of('.'), slash = path.find_last_of('\\');
    if (dot == std::string::npos || (slash != std::string::npos && slash > dot)) return false; // a dot in a folder's name only
    std::string ext = indexer::foldCase(path.substr(dot));
    return ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".msc";
}

Chord keys(int key, bool ctrl, bool shift, bool alt = false) { return Chord{ .key = key, .ctrl = ctrl, .alt = alt, .shift = shift }; }

} // namespace

std::string pathOf(const SearchProvider::Result& r)
{
    if (r.file) return r.target;
    if (r.kind == Launchable::Kind::App && r.package.empty()) return r.location;
    return {};
}

bool elevates(const SearchProvider::Result& r)
{
    if (r.file) return !r.folder && runnableFile(r.target);
    return r.kind == Launchable::Kind::App && r.package.empty();
}

std::vector<MenuItem> resultMenu(const SearchProvider::Result& r, std::size_t index)
{
    std::string n = std::to_string(index);
    std::string path = pathOf(r);
    bool app = !r.file && r.kind == Launchable::Kind::App;
    std::vector<MenuItem> m;
    m.push_back({ .label = "Open", .actionId = A::kOpen + n, .chord = keys(kEnter, false, false), .bold = true, .group = 0 });
    if (elevates(r))
        m.push_back({ .label = "Run as administrator", .actionId = A::kAdmin + n, .chord = keys(kEnter, true, true), .group = 0 });
    if (!path.empty()) {
        m.push_back({ .label = "Reveal in Explorer", .actionId = A::kReveal + n, .chord = keys('E', true, true), .group = 1 });
        m.push_back({ .label = "Open in Terminal", .actionId = A::kTerminal + n, .chord = keys('T', true, true), .group = 1 });
    }
    if (r.file) m.push_back({ .label = "Copy", .actionId = A::kCopy + n, .chord = keys('C', true, false), .group = 2 });
    // A Settings page's path is its link: pasted into Run or a browser, it opens it.
    if (!path.empty() || r.kind == Launchable::Kind::Setting)
        m.push_back({ .label = "Copy path", .actionId = A::kCopyPath + n, .chord = keys('C', true, true), .group = 2 });
    if (r.remembered)
        m.push_back({ .label = "Remove from history", .actionId = A::kForget + n, .chord = keys(kDelete, false, true), .group = 3 });
    if (app && !r.package.empty()) m.push_back({ .label = "App settings", .actionId = A::kAppSettings + n, .group = 4 });
    if (app) m.push_back({ .label = "Uninstall", .actionId = A::kUninstall + n, .group = 4 });
    if (!path.empty()) m.push_back({ .label = "Properties", .actionId = A::kProperties + n, .chord = keys(kEnter, false, false, true), .group = 5 });
    return m;
}

} // namespace cup
