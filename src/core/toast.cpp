#include "core/toast.h"

#include "core/item_metrics.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {

const Toast* toastOn(const IslandState& s, const std::string& activityId)
{
    return s.toast && s.toast->activityId == activityId ? &*s.toast : nullptr;
}

ToastScene placeToast(const Toast& toast, const Source& source, Vec2 cardSize, float bottom, Ms now, const MeasureText& measure)
{
    const Tokens& t = tokens();
    Text text{ .value = toast.text, .style = TextStyle::Caption, .centred = true };
    Row note{ .items = { text }, .align = Align::Center, .hug = true };
    float width = (measure ? measure(text) : estimateTextWidth(text)) + 1, height = lineHeight(TextStyle::Caption);
    if (!toast.actionLabel.empty()) {
        Button action{ .actionId = toast.actionId, .label = toast.actionLabel, .compact = true };
        width += t.itemSpacing + fixedWidthOf(action, t.smallButtonSize, measure).value_or(0);
        height = std::max(height, t.smallButtonSize);
        note.items.push_back(action);
    }
    Vec2 pad = t.toastPadding;
    float w = std::min(width + 2 * pad.x, std::max(0.0f, cardSize.x - 2 * t.expandedPadding));
    float h = height + 2 * pad.y;
    ToastScene scene{ .frame = { (cardSize.x - w) / 2, bottom - h, w, h }, .radius = h / 2 };
    Slot slot{ note };
    Rect inner{ scene.frame.x + pad.x, scene.frame.y + pad.y, std::max(0.0f, w - 2 * pad.x), height };
    placeSlot({ "toast", &slot, inner }, source, std::max(0.0f, scene.radius - pad.y), now, scene.elements, measure);
    return scene;
}

} // namespace cup
