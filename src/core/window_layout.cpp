#include "core/window_layout.h"

#include "core/badges.h"
#include "core/groups.h"
#include "core/slot_layout.h"
#include "core/toast.h"
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
    s.now = now;
    Source source{ .providerId = w.key.providerId };

    // The title bar: our own, so it's the window's colour; Windows' buttons drawn by us at its right.
    // Whole pixels for the areas' edges: tokens are scaled by the display's
    // scale (decision 017), and a pane at 237.5 px blurred every item in it.
    float bar = std::round(t.windowTitleBarHeight), button = std::round(t.windowChromeButtonWidth);
    s.titleBar = { 0, 0, size.x, bar };
    bool maximizes = w.spec.resizable && w.spec.maximizable;
    float buttons = maximizes ? 3 : 2;
    s.chrome.push_back(placedButton("chrome/min/0", Glyph::Minimize, kWindowMinimize, { size.x - buttons * button, 0, button, bar }, w));
    if (maximizes)
        s.chrome.push_back(placedButton("chrome/max/0", maximized ? Glyph::Restore : Glyph::Maximize, kWindowMaximize,
                                        { size.x - 2 * button, 0, button, bar }, w));
    s.chrome.push_back(placedButton("chrome/close/0", Glyph::CloseWindow, kWindowClose, { size.x - button, 0, button, bar }, w));

    float left = w.content.pages.size() > 1 ? placeSidebar(s, w, bar, now, measure) : 0;
    // No sidebar header to name the window: its title in the bar, as Windows puts one.
    if (left == 0 || w.content.rail) {
        Slot title = windowStyled({ row({ Text{ .value = w.content.title, .style = TextStyle::Caption } }) });
        // As wide as its text, not the bar: a resize then doesn't repaint it (decision 120).
        float room = std::max(0.0f, size.x - buttons * button - 2 * t.windowSidebarInset);
        const Text& text = std::get<Text>(title.front().items.front());
        float width = measure ? std::min(room, std::ceil(measure(text)) + 2) : room;
        placeSlot({ "chrome/title", &title, { t.windowSidebarInset + 4, 0, width, bar } }, source, 0, now, s.chrome, measure);
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
    PlacedPage placed = placePage(*page, s.pane.width, fields, now, measure, s.page, w.content.accent);
    float content = placed.height;
    s.headerRule = placed.rule;
    s.movables = std::move(placed.movables);
    s.dropZones = std::move(placed.dropZones);
    // The window's accent on its live data, as a card's on its own (decision 010): a step, a ring, a bar.
    for (std::vector<Placed>* items : { &s.page, &s.footer })
        for (Placed& p : *items)
            if (!p.accent) p.accent = w.content.accent;
    // A button's dot (a setting with something new inside, decision 048), where the island puts one.
    Scene dots;
    dots.size = { s.pane.width, content };
    dots.elements = s.page;
    s.badges = badgeCentres(dots);
    s.maxScroll = std::max(0.0f, content - s.pane.height);
    s.scroll = std::clamp(scroll, 0.0f, s.maxScroll);
    // Its toast (decision 121): a pill at the page's bottom, the island's own (toast.h), over the page.
    if (w.toast) {
        s.toast = placeToast(*w.toast, fields, { s.pane.width, s.pane.height }, s.pane.height - t.windowPagePadding.y, now, measure);
        s.toastUntil = w.toast->until;
    }
    // Its question (decision 122): over everything under the title bar, which stays usable.
    if (w.dialog) s.dialog = placeWindowDialog(*w.dialog, size, bar, fields, now, measure);
    return s;
}

Vec2 toPage(const WindowScene& s, Vec2 point) { return { point.x - s.pane.x, point.y - s.pane.y + s.scroll }; }

std::string windowCardAt(const WindowScene& s, Vec2 point)
{
    if (s.dialog || !s.pane.contains(point)) return {};
    Vec2 at = toPage(s, point);
    for (const Placed& p : s.page)
        if (std::holds_alternative<GroupFill>(p.item) && p.key.ends_with("/fill") && p.frame.contains(at)) return p.key.substr(0, p.key.size() - 5);
    return {};
}

std::optional<Ms> windowDeadline(const WindowScene& s)
{
    Scene page;
    page.size = { s.pane.width, s.pane.height };
    page.elements = s.page;
    page.now = s.now;
    std::optional<Ms> next = sceneDeadline(page);
    if (s.toastUntil && (!next || *s.toastUntil < *next)) next = s.toastUntil;
    return next;
}

std::string windowRowAt(const WindowScene& s, Vec2 point)
{
    if (s.dialog) return {}; // under a question nothing is lit (decision 122)
    if (s.toast && s.toast->frame.contains({ point.x - s.pane.x, point.y - s.pane.y })) return {}; // over the toast: no row under it
    if (!s.pane.contains(point)) return {};
    Vec2 at = toPage(s, point);
    auto prefix = [](const std::string& key) { return key.substr(0, key.rfind('/')); };
    auto slotOf = [](const std::string& key) { return key.substr(0, key.find('/')); };
    auto isFill = [](const Placed& p) { return std::holds_alternative<GroupFill>(p.item); };
    // A card's drag handle (decision 129) is no row: over it, it named the card, and the card's last row lit (decision 134).
    auto notRow = [&](const Placed& p) { return isFill(p) || p.key.ends_with("/handle"); };
    // A card's background (decision 120) says which card; a grouped row's piece is that row.
    std::string card;
    for (const Placed& p : s.page) {
        if (!isFill(p) || !p.frame.contains(at)) continue;
        if (p.key.size() > 5 && p.key.ends_with("/fill")) card = prefix(p.key);
        else return prefix(p.key);
    }
    for (const Placed& p : s.page)
        if (!notRow(p) && p.hitFrame.contains(at)) return prefix(p.key);
    // Between its items: the band their click areas span, in this card if over one, and in no card
    // if over none (the gap beside a card is no row of it, decision 122).
    auto inCard = [](const std::string& slot) { return slot.rfind("card.", 0) == 0; };
    for (const Placed& p : s.page) {
        if (notRow(p) || at.y < p.hitFrame.y || at.y >= p.hitFrame.y + p.hitFrame.height) continue;
        if (p.listRow && (at.x < p.listRow->x || at.x >= p.listRow->x + p.listRow->width)) continue; // a hugging row: its lit part only
        std::string slot = slotOf(p.key);
        if (card.empty() ? !inCard(slot) : slot == card) return prefix(p.key);
    }
    return {};
}

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
    if (s.dialog) { // a question: only its buttons take a click, nothing under it (decision 122)
        auto hit = stamped(hitIn(s.dialog->elements, point));
        return hit ? WindowHit{ WindowPart::Content, hit } : WindowHit{};
    }
    if (s.toast) { // over the page: its button first, the rest of its pill does nothing
        Vec2 inPane{ point.x - s.pane.x, point.y - s.pane.y };
        if (s.toast->frame.contains(inPane)) {
            auto hit = stamped(hitIn(s.toast->elements, inPane));
            return hit ? WindowHit{ WindowPart::Content, hit } : WindowHit{};
        }
    }
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
