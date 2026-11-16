#include "core/window_layout.h"

#include "core/badges.h"
#include "core/groups.h"
#include "core/slot_layout.h"
#include "core/tokens.h"
#include "core/window_fields.h"
#include "core/window_page.h"

#include <algorithm>
#include <cmath>

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

// The sidebar: the window's title over an entry per page, each its glyph
// and name; or, as a rail, the glyphs alone (decision 117). Pages atFoot
// stack up from its bottom.
float placeSidebar(WindowScene& s, const OpenWindow& w, float bar, Ms now, const MeasureText& measure)
{
    const Tokens& t = tokens();
    const auto& pages = w.content.pages;
    Source source{ .providerId = w.key.providerId };
    bool rail = w.content.rail;
    float left = std::round(std::min(rail ? t.windowRailWidth : t.windowSidebarWidth, s.size.x / 2));
    s.sidebar = Rect{ 0, bar, left, std::max(0.0f, s.size.y - bar) };
    s.rail = rail;
    float inset = t.windowSidebarInset, entryWidth = rail ? std::min(t.windowNavHeight, left - 2 * inset) : std::max(0.0f, left - 2 * inset);
    float entryX = rail ? std::round((left - entryWidth) / 2) : inset;
    float top = bar + inset;
    if (!rail) {
        Slot header = windowStyled({ row({ Text{ .value = w.content.title, .style = TextStyle::Title } }) });
        placeSlot({ "side", &header, { inset + 10, bar, std::max(0.0f, left - 2 * inset - 10), t.windowSidebarHeader } }, source, 0, now,
                  s.sidebarItems, measure);
        top = bar + t.windowSidebarHeader;
    }
    std::size_t atFoot = static_cast<std::size_t>(std::count_if(pages.begin(), pages.end(), [](const WindowPage& p) { return p.atFoot; }));
    float foot = s.size.y - inset - static_cast<float>(atFoot) * (t.windowNavHeight + 4) + 4;
    for (std::size_t i = 0; i < pages.size(); ++i) {
        const WindowPage& page = pages[i];
        float& y = page.atFoot ? foot : top;
        Rect entry{ entryX, std::round(y), entryWidth, t.windowNavHeight };
        Row one{ .items = { Icon{ page.glyph } }, .align = rail ? Align::Center : Align::Start, .inset = !rail };
        if (!rail) one.items.push_back(Text{ page.title });
        Slot slot = windowStyled({ one });
        placeSlot({ "nav" + std::to_string(i), &slot, entry, rail }, source, t.windowNavRadius, now, s.sidebarItems, measure);
        s.nav.push_back({ entry, page.id, page.id == w.page, page.badge });
        y += t.windowNavHeight + 4;
    }
    return left;
}

} // namespace

WindowScene windowLayout(const OpenWindow& w, Vec2 size, float scroll, Ms now, const MeasureText& measure, const WindowTyping* typing,
                         bool maximized)
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
    // Whole pixels for the areas' edges: tokens are scaled by the display's
    // scale (decision 017), and a pane at 237.5 px blurred every item in it.
    float bar = std::round(t.windowTitleBarHeight), button = std::round(t.windowChromeButtonWidth);
    s.titleBar = { 0, 0, size.x, bar };
    bool maximizes = w.spec.resizable && w.spec.maximizable;
    float buttons = maximizes ? 3 : 2;
    s.chrome.push_back(placedButton("chrome/min/0", Glyph::Minus, kWindowMinimize, { size.x - buttons * button, 0, button, bar }, w));
    if (maximizes)
        s.chrome.push_back(placedButton("chrome/max/0", maximized ? Glyph::Restore : Glyph::Maximize, kWindowMaximize,
                                        { size.x - 2 * button, 0, button, bar }, w));
    s.chrome.push_back(placedButton("chrome/close/0", Glyph::Close, kWindowClose, { size.x - button, 0, button, bar }, w));

    float left = w.content.pages.size() > 1 ? placeSidebar(s, w, bar, now, measure) : 0;
    // No sidebar header to name the window: its title in the bar, as Windows puts one.
    if (left == 0 || w.content.rail) {
        Slot title = windowStyled({ row({ Text{ .value = w.content.title, .style = TextStyle::Caption } }) });
        placeSlot({ "chrome/title", &title, { t.windowSidebarInset + 4, 0, std::max(0.0f, size.x - buttons * button - 2 * t.windowSidebarInset), bar } },
                  source, 0, now, s.chrome, measure);
    }

    // The footer, pinned under the page.
    float bottom = size.y;
    Slot footer = windowStyled(w.content.footer);
    if (!footer.empty()) {
        Rect area{ left + t.windowPagePadding.x, 0, std::max(0.0f, size.x - left - 2 * t.windowPagePadding.x), 0 };
        float height = slotHeight(footer, area, source, measure) + 2 * t.groupItemPadding;
        bottom = std::round(std::max(bar, size.y - height));
        s.footerBar = Rect{ left, bottom, size.x - left, size.y - bottom };
        area.y = bottom;
        area.height = height;
        placeSlot({ "footer", &footer, area }, source, t.windowNavRadius, now, s.footer, measure);
    }
    s.pane = { left, bar, std::max(0.0f, size.x - left), std::max(0.0f, bottom - bar) };

    const WindowPage* page = shownPage(w);
    if (!page) return s;
    // Its fields, as a card's (decision 105): a draft names the window as its card.
    Source fields{ .providerId = w.key.providerId, .activityId = w.key.windowId };
    if (typing) {
        fields.draft = typing->draft ? &*typing->draft : nullptr;
        fields.keptText = &typing->kept;
    }
    s.backAction = page->backAction;
    float content = placePage(*page, s.pane.width, fields, now, measure, s.page, s.headerRule);
    // A button's dot (a setting with something new inside, decision 048), where the island puts one.
    Scene dots;
    dots.size = { s.pane.width, content };
    dots.elements = s.page;
    s.badges = badgeCentres(dots);
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
    for (const Placed& p : s.chrome) {
        auto* button = std::get_if<Button>(&p.item); // the title beside them is only text
        if (!button || !p.hitFrame.contains(point)) continue;
        WindowPart part = button->actionId == kWindowClose ? WindowPart::Close : button->actionId == kWindowMaximize ? WindowPart::Maximize : WindowPart::Minimize;
        return { part, Hit{ {}, p.key }, {} };
    }
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
