#include "core/window_keys.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// The page's or footer's items as a card's scene, for the island's own rules.
Scene asScene(const std::vector<Placed>& elements, Vec2 size)
{
    Scene s;
    s.size = size;
    s.elements = elements;
    return s;
}

Scene pageScene(const WindowScene& w) { return asScene(w.page, { w.pane.width, w.pane.height + w.maxScroll }); }
Scene footerScene(const WindowScene& w) { return asScene(w.footer, w.size); }

WindowFocus fromPage(const WindowScene& w, const Focusable& f)
{
    Rect shown{ f.frame.x + w.pane.x, f.frame.y + w.pane.y - w.scroll, f.frame.width, f.frame.height };
    return { "el:" + f.id.substr(3), shown, f.radius, f.frame, f.point, true };
}

WindowFocus fromFooter(const Focusable& f) { return { "foot:" + f.id.substr(3), f.frame, f.radius, {}, f.point, false }; }

WindowFocus fromNav(const WindowScene& w, const NavEntry& n)
{
    return { "nav:" + n.page, n.frame, w.navRadius, {}, { n.frame.x + n.frame.width / 2, n.frame.y + n.frame.height / 2 }, false };
}

std::vector<WindowFocus> pageStops(const WindowScene& w)
{
    std::vector<WindowFocus> out;
    for (const Focusable& f : focusables(pageScene(w))) out.push_back(fromPage(w, f));
    return out;
}

std::vector<WindowFocus> footerStops(const WindowScene& w)
{
    std::vector<WindowFocus> out;
    for (const Focusable& f : focusables(footerScene(w))) out.push_back(fromFooter(f));
    return out;
}

// The sidebar's picked page: where Left from the page lands.
std::optional<WindowFocus> pickedNav(const WindowScene& w)
{
    for (const NavEntry& n : w.nav)
        if (n.picked) return fromNav(w, n);
    return w.nav.empty() ? std::nullopt : std::optional<WindowFocus>(fromNav(w, w.nav.front()));
}

// The page's stop nearest the top of what shows: where Right from the sidebar lands.
std::optional<WindowFocus> firstShown(const WindowScene& w)
{
    auto stops = pageStops(w);
    for (const WindowFocus& f : stops)
        if (f.pageFrame.y + f.pageFrame.height > w.scroll) return f;
    return stops.empty() ? std::nullopt : std::optional<WindowFocus>(stops.front());
}

std::string keyOf(const std::string& id, const char* prefix) { return id.substr(std::string(prefix).size()); }

} // namespace

std::vector<WindowFocus> windowFocusables(const WindowScene& w)
{
    std::vector<WindowFocus> out;
    for (const NavEntry& n : w.nav) out.push_back(fromNav(w, n));
    for (WindowFocus& f : pageStops(w)) out.push_back(std::move(f));
    for (WindowFocus& f : footerStops(w)) out.push_back(std::move(f));
    return out;
}

std::optional<WindowFocus> findWindowFocus(const WindowScene& w, const std::string& id)
{
    if (id.empty()) return std::nullopt;
    for (const WindowFocus& f : windowFocusables(w))
        if (f.id == id) return f;
    return std::nullopt;
}

std::optional<WindowFocus> moveWindowFocus(const WindowScene& w, const std::string& current, Arrow arrow)
{
    auto at = findWindowFocus(w, current);
    if (!at) {
        if (arrow == Arrow::Left) return pickedNav(w);
        auto stops = pageStops(w);
        if (!stops.empty()) return stops.front();
        return pickedNav(w);
    }
    if (at->id.rfind("nav:", 0) == 0) {
        if (arrow == Arrow::Right) return firstShown(w);
        if (arrow == Arrow::Left) return std::nullopt;
        for (std::size_t i = 0; i < w.nav.size(); ++i) {
            if ("nav:" + w.nav[i].page != at->id) continue;
            if (arrow == Arrow::Up && i > 0) return fromNav(w, w.nav[i - 1]);
            if (arrow == Arrow::Down && i + 1 < w.nav.size()) return fromNav(w, w.nav[i + 1]);
        }
        return std::nullopt;
    }
    if (at->id.rfind("foot:", 0) == 0) {
        if (auto next = moveFocus(footerScene(w), "el:" + keyOf(at->id, "foot:"), arrow)) return fromFooter(*next);
        if (arrow == Arrow::Up) {
            auto stops = pageStops(w);
            if (!stops.empty()) return stops.back();
        }
        return std::nullopt;
    }
    // On the page: as on a card; off its left edge, the sidebar; off its bottom, the footer.
    if (auto next = moveFocus(pageScene(w), "el:" + keyOf(at->id, "el:"), arrow)) return fromPage(w, *next);
    if (arrow == Arrow::Left) return pickedNav(w);
    if (arrow == Arrow::Down) {
        auto stops = footerStops(w);
        if (!stops.empty()) return stops.front();
    }
    return std::nullopt;
}

std::optional<WindowFocus> stepWindowFocus(const WindowScene& w, const std::string& current, bool backwards)
{
    auto all = windowFocusables(w);
    if (all.empty()) return std::nullopt;
    auto it = std::find_if(all.begin(), all.end(), [&](const WindowFocus& f) { return f.id == current; });
    if (it == all.end()) return backwards ? all.back() : all.front();
    std::size_t i = static_cast<std::size_t>(it - all.begin()), n = all.size();
    return all[backwards ? (i + n - 1) % n : (i + 1) % n];
}

std::optional<WindowFocus> pageEndFocus(const WindowScene& w, bool last)
{
    auto stops = pageStops(w);
    if (stops.empty()) return std::nullopt;
    return last ? stops.back() : stops.front();
}

WindowHit activateFocus(const WindowScene& w, const WindowFocus& focus)
{
    auto stamped = [&](std::optional<Hit> hit) -> WindowHit {
        if (!hit) return {};
        hit->action.activityId.clear();
        hit->action.windowId = w.windowId;
        return { WindowPart::Content, hit, {} };
    };
    if (focus.id.rfind("nav:", 0) == 0) return { WindowPart::Page, std::nullopt, keyOf(focus.id, "nav:") };
    if (focus.id.rfind("foot:", 0) == 0) return stamped(hitIn(w.footer, focus.point));
    return stamped(hitIn(w.page, focus.point));
}

float scrollToShow(const WindowScene& w, const WindowFocus& focus)
{
    if (!focus.inPage) return w.scroll;
    float margin = std::min(tokens().windowPagePadding.y, w.pane.height / 4);
    float top = focus.pageFrame.y - margin, bottom = focus.pageFrame.y + focus.pageFrame.height + margin;
    float scroll = w.scroll;
    if (bottom > scroll + w.pane.height) scroll = bottom - w.pane.height;
    if (top < scroll) scroll = top;
    return std::clamp(scroll, 0.0f, w.maxScroll);
}

} // namespace cup
