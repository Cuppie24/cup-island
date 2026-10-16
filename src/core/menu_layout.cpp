#include "core/menu_layout.h"

#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// Its items as list rows: a glyph and a label, lit and clicked as a whole.
Slot rowsOf(const Menu& menu)
{
    Slot rows;
    for (std::size_t i = 0; i < menu.items.size(); ++i) {
        const MenuItem& item = menu.items[i];
        Button button{ .glyph = item.glyph, .actionId = item.actionId, .compact = true, .destructive = item.destructive, .editsText = item.editsText };
        rows.push_back(Row{ .items = { button, Text{ .value = item.label } }, .clickAnywhere = true, .id = std::to_string(i), .listItem = true });
    }
    return rows;
}

} // namespace

Rect menuFrame(Vec2 at, Vec2 size, Rect card)
{
    float x = at.x + size.x <= card.x + card.width ? at.x : at.x - size.x; // no room on the right: to the left of the pointer
    float y = at.y + size.y <= card.y + card.height ? at.y : at.y - size.y; // no room below: above it
    x = std::clamp(x, card.x, std::max(card.x, card.x + card.width - size.x));
    y = std::clamp(y, card.y, std::max(card.y, card.y + card.height - size.y));
    return { x, y, size.x, size.y };
}

void placeMenu(const IslandState& s, Scene& scene, const MeasureText& measure)
{
    if (!s.menu || scene.mode != Mode::Expanded || scene.activityId != s.menu->activityId) return;
    const Tokens& t = tokens();
    Slot rows = rowsOf(*s.menu);
    float pad = t.menuPadding;
    Rect probe{ 0, 0, t.menuWidth - 2 * pad, scene.size.y };
    Content content{ .label = "Menu" };
    Source source{ &content, s.menu->providerId, s.menu->activityId };
    float height = slotHeight(rows, probe, source, measure) + 2 * pad;
    Rect card{ t.expandedPadding, t.expandedPadding, scene.size.x - 2 * t.expandedPadding, scene.size.y - 2 * t.expandedPadding };
    Rect frame = menuFrame(s.menu->at, { t.menuWidth, height }, card);
    float radius = std::min(std::max(0.0f, pillCornerRadius(scene.size) - t.expandedPadding), frame.height / 2);
    scene.menu = MenuPanel{ frame, radius };
    std::vector<Placed> items;
    placeSlot({ "menu", &rows, { frame.x + pad, frame.y + pad, frame.width - 2 * pad, frame.height - 2 * pad } }, source,
              std::max(0.0f, radius - pad), scene.now, items, measure);
    scene.elements.insert(scene.elements.begin(), items.begin(), items.end());
}

} // namespace cup
