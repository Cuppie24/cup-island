#include "core/toast.h"

#include "core/item_metrics.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

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
    // In whole pixels, padding first, so its button sits as many pixels from the pill's top as from its
    // bottom: frames snapped edge by edge would split an odd spare pixel, a button half a pixel low.
    Vec2 pad{ std::round(t.toastPadding.x), std::round(t.toastPadding.y) };
    height = std::round(height);
    float w = std::round(std::min(width + 2 * pad.x, std::max(0.0f, cardSize.x - 2 * t.expandedPadding)));
    float h = height + 2 * pad.y;
    // Like a toast from a toaster: it comes up from below the card's bottom edge, hidden behind it at first.
    float top = std::round(bottom - h);
    ToastScene scene{ .frame = { std::round((cardSize.x - w) / 2), top, w, h }, .radius = h / 2, .slide = cardSize.y - top };
    Slot slot{ note };
    Rect inner{ scene.frame.x + pad.x, scene.frame.y + pad.y, std::max(0.0f, w - 2 * pad.x), height };
    placeSlot({ "toast", &slot, inner }, source, std::max(0.0f, scene.radius - pad.y), now, scene.elements, measure);
    return scene;
}

} // namespace cup
