#include "core/open_windows.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

OpenWindow* find(Windows& w, const WindowKey& key)
{
    for (OpenWindow& o : w.open)
        if (o.key == key) return &o;
    return nullptr;
}

// Decision 104: a provider shows a window only right after the user acted
// on it, so no window (and no focus) is taken by surprise, from a built-in
// or a plugin alike (PS §9: providers can't take keyboard focus).
bool mayOpen(const Windows& w, const std::string& providerId, Ms now)
{
    auto it = w.actedAt.find(providerId);
    return it != w.actedAt.end() && now >= it->second && now - it->second <= tokens().windowOpenGrace;
}

} // namespace

std::string pageOrFirst(const WindowContent& content, const std::string& wanted)
{
    for (const WindowPage& p : content.pages)
        if (p.id == wanted) return p.id;
    return content.pages.empty() ? std::string() : content.pages.front().id;
}

void userActed(Windows& w, const std::string& providerId, Ms now) { w.actedAt[providerId] = now; }

WindowOpen openWindow(Windows& w, const std::string& providerId, const WindowSpec& spec, WindowContent content, Ms now)
{
    if (!validWindowId(spec.id)) return WindowOpen::BadId;
    if (!mayOpen(w, providerId, now)) return WindowOpen::NotNow;
    WindowKey key{ providerId, spec.id };
    if (OpenWindow* o = find(w, key)) {
        o->spec = spec;
        o->page = pageOrFirst(content, spec.page.empty() ? o->page : spec.page);
        o->content = std::move(content);
        o->raised = w.nextRaise++;
        return WindowOpen::Raised;
    }
    auto mine = std::count_if(w.open.begin(), w.open.end(), [&](const OpenWindow& o) { return o.key.providerId == providerId; });
    if (static_cast<std::size_t>(mine) >= tokens().windowsPerProvider) return WindowOpen::TooMany;
    std::string page = pageOrFirst(content, spec.page);
    w.open.push_back({ key, spec, std::move(content), page, w.nextRaise++ });
    return WindowOpen::Opened;
}

bool updateWindow(Windows& w, const WindowKey& key, WindowContent content)
{
    OpenWindow* o = find(w, key);
    if (!o) return false;
    o->page = pageOrFirst(content, o->page); // its page went: the first
    o->content = std::move(content);
    return true;
}

bool showPage(Windows& w, const WindowKey& key, const std::string& pageId)
{
    OpenWindow* o = find(w, key);
    if (!o || o->page == pageId) return false;
    if (std::none_of(o->content.pages.begin(), o->content.pages.end(), [&](const WindowPage& p) { return p.id == pageId; })) return false;
    o->page = pageId;
    return true;
}

bool closeWindow(Windows& w, const WindowKey& key)
{
    auto before = w.open.size();
    std::erase_if(w.open, [&](const OpenWindow& o) { return o.key == key; });
    return w.open.size() != before;
}

std::vector<WindowKey> closeProviderWindows(Windows& w, const std::string& providerId)
{
    std::vector<WindowKey> closed;
    for (const OpenWindow& o : w.open)
        if (o.key.providerId == providerId) closed.push_back(o.key);
    std::erase_if(w.open, [&](const OpenWindow& o) { return o.key.providerId == providerId; });
    w.actedAt.erase(providerId);
    return closed;
}

const OpenWindow* findWindow(const Windows& w, const WindowKey& key)
{
    for (const OpenWindow& o : w.open)
        if (o.key == key) return &o;
    return nullptr;
}

bool validWindowId(const std::string& id)
{
    if (id.empty()) return false;
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    });
}

} // namespace cup
