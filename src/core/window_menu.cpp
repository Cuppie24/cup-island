#include "core/window_menu.h"

#include "core/menu.h"
#include "core/tokens.h"
#include "core/window_page.h"

#include <algorithm>

namespace cup {
namespace {

const WindowSection* cardOf(const OpenWindow& w, const std::string& section)
{
    for (const WindowPage& page : w.content.pages) {
        if (page.id != w.page) continue;
        for (const WindowSection& s : page.sections)
            if (s.id == section) return &s;
    }
    return nullptr;
}

} // namespace

std::optional<std::string> cardMenuOf(const std::string& actionId)
{
    std::string prefix = kWindowCardMenu;
    if (actionId.rfind(prefix, 0) != 0 || actionId.size() == prefix.size()) return std::nullopt;
    return actionId.substr(prefix.size());
}

std::optional<Menu> openCardMenu(const OpenWindow& w, const std::string& section)
{
    const WindowSection* card = cardOf(w, section);
    if (!card || card->menu.empty()) return std::nullopt;
    return Menu{ .providerId = w.key.providerId, .activityId = w.key.windowId, .row = section, .items = card->menu };
}

std::optional<MenuScene> placeWindowMenu(const OpenWindow& w, const WindowScene& s, const Menu& menu, Ms now, const MeasureText& measure)
{
    const WindowSection* card = cardOf(w, menu.row);
    if (!card || card->menu != menu.items) return std::nullopt; // gone, or another menu now: it closes
    std::string action = kWindowCardMenu + menu.row;
    const Placed* button = nullptr;
    for (const Placed& p : s.page)
        if (auto* b = std::get_if<Button>(&p.item); b && b->actionId == action) button = &p;
    if (!button) return std::nullopt;
    // The button in window coordinates; the menu under it, right edges together, as wide as it needs.
    Rect at{ button->frame.x + s.pane.x, button->frame.y + s.pane.y - s.scroll, button->frame.width, button->frame.height };
    Source source{ .providerId = w.key.providerId, .activityId = w.key.windowId };
    Menu sized = menu;
    sized.at = Vec2{ 0, 0 };
    float width = placeMenu(sized, source, at, s.pane, now, measure).frame.width;
    sized.at = Vec2{ at.x + at.width - width, at.y + at.height + tokens().menuGap };
    return placeMenu(sized, source, at, s.pane, now, measure);
}

std::optional<Action> menuChoice(const WindowScene& s, const Menu& menu, std::size_t index)
{
    if (index >= menu.items.size() || !menu.items[index].enabled) return std::nullopt;
    return Action{ .providerId = s.providerId, .actionId = menu.items[index].actionId, .windowId = s.windowId };
}

} // namespace cup
