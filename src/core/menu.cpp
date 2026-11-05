#include "core/menu.h"

#include "core/dialog.h"
#include "core/item_metrics.h"
#include "core/tokens.h"

#include <algorithm>
#include <cctype>

namespace cup {
namespace {

const Slot* slotNamed(const Content& c, std::string_view name)
{
    struct Named {
        const char* name;
        const Slot Content::*slot;
    };
    static constexpr Named slots[] = { { "expandedCenter", &Content::expandedCenter }, { "expandedLeading", &Content::expandedLeading },
                                       { "expandedTrailing", &Content::expandedTrailing }, { "expandedBottom", &Content::expandedBottom } };
    for (const Named& n : slots)
        if (name == n.name) return &(c.*n.slot);
    return nullptr; // menus are a card's: not on the pill's slots or Home's widgets
}

std::string providerOf(const IslandState& s, const std::string& id)
{
    if (const Activity* a = findActivity(s, id)) return a->providerId;
    const Panel* p = findPanel(s, id);
    return p ? p->providerId : std::string();
}

bool shownOnIsland(const IslandState& s, const std::string& id) { return !s.homeOpen && s.expandedId == id; }

Text labelOf(const MenuItem& item)
{
    Text label{ .value = item.label, .style = item.enabled ? TextStyle::Body : TextStyle::Caption };
    if (item.bold) label.bright = { { 0, item.label.size() } }; // the row's own action: as bold as a match (decision 079)
    return label;
}

Text chordOf(const MenuItem& item) { return Text{ .value = chordText(*item.chord), .style = TextStyle::Caption, .alignEnd = true }; }

} // namespace

std::string rowKeyOf(const std::string& elementKey)
{
    std::size_t slash = elementKey.rfind('/');
    return slash == std::string::npos ? elementKey : elementKey.substr(0, slash);
}

const Row* rowNamed(const Content& content, const std::string& rowKey)
{
    std::size_t slash = rowKey.find('/');
    if (slash == std::string::npos) return nullptr;
    const Slot* slot = slotNamed(content, std::string_view(rowKey).substr(0, slash));
    if (!slot) return nullptr;
    std::string name = rowKey.substr(slash + 1);
    for (std::size_t i = 0; i < slot->size(); ++i) {
        const Row& r = (*slot)[i];
        if ((r.id.empty() ? std::to_string(i) : r.id) == name) return &r; // as layout names rows in element keys
    }
    return nullptr;
}

void openMenu(IslandState& s, const MenuOpened& e, Ms now)
{
    if (!shownOnIsland(s, e.activityId) || dialogOver(s, e.activityId)) return;
    const Content* content = cardContent(s, e.activityId);
    const Row* row = content ? rowNamed(*content, e.row) : nullptr;
    if (!row || row->menu.empty()) return;
    s.card.menu = Menu{ .providerId = providerOf(s, e.activityId), .activityId = e.activityId, .row = e.row, .items = row->menu, .at = e.at };
    if (!e.at) s.card.menu->lit = menuStep(*s.card.menu, 1); // by the keys: they're on its first item
    s.lastPointerAt = now;
}

void lightMenu(IslandState& s, const MenuLit& e, Ms now)
{
    if (!s.card.menu) return;
    Menu& m = *s.card.menu;
    m.lit = e.item && *e.item < m.items.size() && m.items[*e.item].enabled ? e.item : std::nullopt;
    s.lastPointerAt = now;
}

void closeMenu(IslandState& s) { s.card.menu.reset(); }

void dropStaleMenu(IslandState& s)
{
    if (!s.card.menu) return;
    const Menu& m = *s.card.menu;
    const Content* content = shownOnIsland(s, m.activityId) && !dialogOver(s, m.activityId) ? cardContent(s, m.activityId) : nullptr;
    const Row* row = content ? rowNamed(*content, m.row) : nullptr;
    if (!row || row->menu != m.items) s.card.menu.reset();
}

std::optional<std::size_t> menuStep(const Menu& m, int step)
{
    std::size_t n = m.items.size();
    if (n == 0) return std::nullopt;
    // From the lit one; nothing lit, from just outside the end the step comes in from.
    long long at = m.lit ? static_cast<long long>(*m.lit) : step > 0 ? -1 : static_cast<long long>(n);
    for (std::size_t tries = 0; tries < n; ++tries) {
        at = ((at + step) % static_cast<long long>(n) + static_cast<long long>(n)) % static_cast<long long>(n);
        if (m.items[static_cast<std::size_t>(at)].enabled) return static_cast<std::size_t>(at);
    }
    return std::nullopt; // none can be chosen
}

std::optional<std::size_t> menuEnd(const Menu& m, bool last)
{
    Menu from = m;
    from.lit.reset();
    return menuStep(from, last ? -1 : 1);
}

std::optional<std::size_t> menuLetter(const Menu& m, char letter)
{
    std::size_t n = m.items.size();
    char want = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    std::size_t start = m.lit ? *m.lit + 1 : 0;
    for (std::size_t k = 0; k < n; ++k) {
        const MenuItem& item = m.items[(start + k) % n];
        if (item.enabled && !item.label.empty() && std::toupper(static_cast<unsigned char>(item.label[0])) == want) return (start + k) % n;
    }
    return std::nullopt;
}

const MenuItem* itemForChord(const Content& content, const std::string& rowKey, const Chord& chord)
{
    const Row* row = rowNamed(content, rowKey);
    if (!row) return nullptr;
    for (const MenuItem& item : row->menu)
        if (item.enabled && item.chord && *item.chord == chord) return &item;
    return nullptr;
}

Rect rowFrame(const Scene& scene, const std::string& rowKey)
{
    float left = 0, top = 0, right = 0, bottom = 0;
    bool any = false;
    for (const Placed& p : scene.elements) {
        if (rowKeyOf(p.key) != rowKey) continue;
        Rect r = p.wholeRow ? *p.wholeRow : p.frame;
        if (!any) left = r.x, top = r.y, right = r.x + r.width, bottom = r.y + r.height;
        left = std::min(left, r.x), top = std::min(top, r.y);
        right = std::max(right, r.x + r.width), bottom = std::max(bottom, r.y + r.height);
        any = true;
    }
    return any ? Rect{ left, top, right - left, bottom - top } : Rect{};
}

MenuScene placeMenu(const Menu& menu, const Source& source, Rect row, Rect bounds, Ms now, const MeasureText& measure)
{
    const Tokens& t = tokens();
    auto width = [&](const Text& text) { return measure ? measure(text) : estimateTextWidth(text); };
    // As wide as its longest label and shortcut, within menuMinWidth..menuMaxWidth.
    float widest = 0;
    std::size_t lines = 0;
    for (std::size_t i = 0; i < menu.items.size(); ++i) {
        const MenuItem& item = menu.items[i];
        float w = width(labelOf(item));
        if (item.chord) w += t.looseItemSpacing + width(chordOf(item));
        widest = std::max(widest, w);
        if (i > 0 && item.group != menu.items[i - 1].group) ++lines;
    }
    float w = std::clamp(widest + 2 * (t.menuPadding + t.menuItemInset), t.menuMinWidth, std::max(t.menuMinWidth, t.menuMaxWidth));
    float h = 2 * t.menuPadding + static_cast<float>(menu.items.size()) * t.menuItemHeight + static_cast<float>(lines) * t.menuLineGap;

    // Where it opens, then kept on the screen's band the render layer covers.
    float right = bounds.x + bounds.width, bottom = bounds.y + bounds.height;
    Vec2 from;
    float x = 0, y = 0;
    if (menu.at) {
        from = *menu.at;
        x = from.x + w <= right ? from.x : from.x - w; // no room to the right: it opens to the pointer's left
        y = from.y;
    } else {
        x = row.x + row.width + t.menuGap;
        from = { x, row.y };
        if (x + w > right) { // no room beside the row on the right: on its left
            x = row.x - t.menuGap - w;
            from = { row.x - t.menuGap, row.y };
        }
        y = row.y;
    }
    x = std::clamp(x, bounds.x, std::max(bounds.x, right - w));
    y = std::clamp(y, bounds.y, std::max(bounds.y, bottom - h));

    MenuScene scene{ .activityId = menu.activityId, .row = menu.row, .frame = { x, y, w, h }, .radius = t.menuRadius, .from = from,
                     .lit = menu.lit };
    float itemY = y + t.menuPadding;
    float itemRadius = std::max(0.0f, t.menuRadius - t.menuPadding); // nested in the menu's corners (decision 006)
    for (std::size_t i = 0; i < menu.items.size(); ++i) {
        const MenuItem& item = menu.items[i];
        if (i > 0 && item.group != menu.items[i - 1].group) {
            float lineY = itemY + (t.menuLineGap - t.dividerThickness) / 2;
            scene.lines.push_back({ x + t.menuPadding + t.menuItemInset, lineY, w - 2 * (t.menuPadding + t.menuItemInset), t.dividerThickness });
            itemY += t.menuLineGap;
        }
        Rect band{ x + t.menuPadding, itemY, w - 2 * t.menuPadding, t.menuItemHeight };
        scene.items.push_back(band);
        scene.enabled.push_back(item.enabled);
        Row r{ .items = { labelOf(item) } };
        if (item.chord) r.trailing.push_back(chordOf(item));
        Slot slot{ r };
        Rect inner{ band.x + t.menuItemInset, band.y, std::max(0.0f, band.width - 2 * t.menuItemInset), band.height };
        placeSlot({ "menu/" + std::to_string(i), &slot, inner }, source, itemRadius, now, scene.elements, measure);
        itemY += t.menuItemHeight;
    }
    return scene;
}

std::optional<std::size_t> menuItemAt(const MenuScene& menu, Vec2 point)
{
    for (std::size_t i = 0; i < menu.items.size(); ++i)
        if (menu.items[i].contains(point)) return menu.enabled[i] ? std::optional<std::size_t>(i) : std::nullopt;
    return std::nullopt;
}

} // namespace cup
