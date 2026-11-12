#include "core/window_layout.h"

#include "core/groups.h"
#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

const WindowPage* shownPage(const OpenWindow& w)
{
    for (const WindowPage& p : w.content.pages)
        if (p.id == w.page) return &p;
    return w.content.pages.empty() ? nullptr : &w.content.pages.front();
}

Placed placedButton(const std::string& key, Glyph glyph, const char* action, Rect frame, const OpenWindow& w)
{
    Placed p;
    p.key = key;
    p.item = Button{ .glyph = glyph, .actionId = action, .compact = true };
    p.frame = p.hitFrame = frame;
    p.providerId = w.key.providerId;
    return p;
}

} // namespace

WindowScene windowLayout(const OpenWindow& w, Vec2 size, float scroll, Ms now, const MeasureText& measure)
{
    const Tokens& t = tokens();
    WindowScene s;
    s.size = size;
    s.title = w.content.title;
    s.providerId = w.key.providerId;
    s.windowId = w.key.windowId;
    s.navRadius = t.windowNavRadius;
    Source source{ .providerId = w.key.providerId };

    // The title bar: our own, so it's the window's colour; Windows' buttons drawn by us at its right.
    float bar = t.windowTitleBarHeight, button = t.windowChromeButtonWidth;
    s.titleBar = { 0, 0, size.x, bar };
    s.chrome.push_back(placedButton("chrome/min/0", Glyph::Minus, kWindowMinimize, { size.x - 2 * button, 0, button, bar }, w));
    s.chrome.push_back(placedButton("chrome/close/0", Glyph::Close, kWindowClose, { size.x - button, 0, button, bar }, w));

    // The sidebar, with several pages: the window's title over an entry per page.
    float left = 0;
    if (w.content.pages.size() > 1) {
        left = std::min(t.windowSidebarWidth, size.x / 2);
        s.sidebar = Rect{ 0, bar, left, std::max(0.0f, size.y - bar) };
        float inset = t.windowSidebarInset;
        Slot header{ row({ Text{ .value = w.content.title, .style = TextStyle::Title } }) };
        placeSlot({ "side", &header, { inset + 10, bar, std::max(0.0f, left - 2 * inset - 10), t.windowSidebarHeader } }, source, 0, now,
                  s.sidebarItems, measure);
        float y = bar + t.windowSidebarHeader;
        for (std::size_t i = 0; i < w.content.pages.size(); ++i) {
            const WindowPage& page = w.content.pages[i];
            Rect entry{ inset, y, std::max(0.0f, left - 2 * inset), t.windowNavHeight };
            Slot one{ Row{ .items = { Icon{ page.glyph }, Text{ page.title } }, .inset = true } };
            placeSlot({ "nav" + std::to_string(i), &one, entry }, source, t.windowNavRadius, now, s.sidebarItems, measure);
            s.nav.push_back({ entry, page.id, page.id == w.page, page.badge });
            y += t.windowNavHeight + 4;
        }
    }

    // The footer, pinned under the page.
    float bottom = size.y;
    if (!w.content.footer.empty()) {
        Rect area{ left + t.windowPagePadding.x, 0, std::max(0.0f, size.x - left - 2 * t.windowPagePadding.x), 0 };
        float height = slotHeight(w.content.footer, area, source, measure) + 2 * t.groupItemPadding;
        bottom = std::max(bar, size.y - height);
        s.footerBar = Rect{ left, bottom, size.x - left, size.y - bottom };
        area.y = bottom;
        area.height = height;
        placeSlot({ "footer", &w.content.footer, area }, source, t.windowNavRadius, now, s.footer, measure);
    }
    s.pane = { left, bar, std::max(0.0f, size.x - left), std::max(0.0f, bottom - bar) };

    // The page: its title (a back arrow before it on a page within a page), a
    // line under it, then each section as a group of rows (core/groups.h): one
    // slot, laid out like a List card's, from the top.
    const WindowPage* page = shownPage(w);
    if (!page) return s;
    s.backAction = page->backAction;
    Slot rows;
    Row title{ .items = { Text{ .value = page->title, .style = TextStyle::PageTitle } } };
    if (!page->backAction.empty()) title.leading = { Button{ .glyph = Glyph::ChevronLeft, .actionId = page->backAction, .compact = true } };
    rows.push_back(title);
    if (!page->subtitle.empty()) rows.push_back(row({ Text{ .value = page->subtitle, .style = TextStyle::Caption } }));
    for (const WindowSection& section : page->sections) {
        std::vector<Row> group = section.rows;
        for (Row& r : group) {
            r.list.clear(); // the page scrolls as a whole (window_content.h)
            r.visibleRows = 0;
        }
        addGroup(rows, section.title, std::move(group));
    }
    float width = std::max(0.0f, std::min(t.windowPageMaxWidth, s.pane.width - 2 * t.windowPagePadding.x));
    Rect frame{ t.windowPagePadding.x, t.windowPagePadding.y, width, 0 };
    frame.height = slotHeight(rows, frame, source, measure);
    placeSlot({ "page", &rows, frame }, source, t.windowNavRadius, now, s.page, measure);
    float y = frame.y + frame.height;
    float content = y + t.windowPagePadding.y;
    s.maxScroll = std::max(0.0f, content - s.pane.height);
    s.scroll = std::clamp(scroll, 0.0f, s.maxScroll);
    return s;
}

Vec2 toPage(const WindowScene& s, Vec2 point) { return { point.x - s.pane.x, point.y - s.pane.y + s.scroll }; }

WindowHit windowHitTest(const WindowScene& s, Vec2 point)
{
    auto stamped = [&](std::optional<Hit> hit) {
        if (hit) {
            hit->action.activityId.clear();
            hit->action.windowId = s.windowId;
        }
        return hit;
    };
    for (const Placed& p : s.chrome)
        if (p.hitFrame.contains(point))
            return { std::get<Button>(p.item).actionId == kWindowClose ? WindowPart::Close : WindowPart::Minimize, Hit{ {}, p.key }, {} };
    if (s.titleBar.contains(point)) return { WindowPart::Caption };
    for (const NavEntry& n : s.nav)
        if (n.frame.contains(point)) return { WindowPart::Page, std::nullopt, n.page };
    if (s.footerBar && s.footerBar->contains(point)) {
        auto hit = stamped(hitIn(s.footer, point));
        return hit ? WindowHit{ WindowPart::Content, hit } : WindowHit{};
    }
    if (!s.pane.contains(point)) return {};
    auto hit = stamped(hitIn(s.page, toPage(s, point)));
    return hit ? WindowHit{ WindowPart::Content, hit } : WindowHit{};
}

} // namespace cup
