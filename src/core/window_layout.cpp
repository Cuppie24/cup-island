#include "core/window_layout.h"

#include "core/dialog.h"
#include "core/slot_layout.h"
#include "core/toast.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// The title bar: the card's label at the left, the ✕ at the right, both
// centred in its height. The rest of the bar drags the window (Scene::titleBar).
void placeTitleBar(const FloatingWindow& w, const std::string& title, Scene& scene)
{
    const Tokens& t = tokens();
    float bar = t.windowTitleBarHeight, pad = t.expandedPadding, side = t.buttonSize;
    scene.titleBar = { 0, 0, w.size.x, bar };
    Rect close{ w.size.x - pad - side, (bar - side) / 2, side, side };
    Rect label{ pad, 0, std::max(0.0f, close.x - t.slotGap - pad), bar };
    scene.elements.push_back({ .key = "window/title", .item = Text{ .value = title, .style = TextStyle::Title }, .frame = label,
                               .hitFrame = label, .providerId = w.providerId, .activityId = w.activityId });
    scene.elements.push_back({ .key = "window/close", .item = Button{ .glyph = Glyph::Close, .actionId = kCloseWindow, .secondary = true },
                               .frame = close, .hitFrame = close, .cornerRadius = side / 2, .providerId = w.providerId,
                               .activityId = w.activityId });
}

} // namespace

Scene windowScene(const IslandState& s, const FloatingWindow& w, Ms now, const MeasureText& measure)
{
    const Tokens& t = tokens();
    MeasureText measureText = measure ? measure : MeasureText{ estimateTextWidth };
    Scene scene;
    scene.now = now;
    scene.mode = Mode::Expanded;
    scene.size = w.size;
    scene.activityId = w.activityId;
    const Content* content = cardContent(s, w.activityId);
    if (!content) return scene;
    scene.label = content->label;
    placeTitleBar(w, content->label, scene);

    // The card under the bar: what the island's card would show, at the
    // window's size. A question takes its place, like on the island (decision 020).
    const Draft* draft = s.draft && s.draft->activityId == w.activityId ? &*s.draft : nullptr;
    const ListDrag* drag = s.listDrag && s.listDrag->activityId == w.activityId ? &*s.listDrag : nullptr;
    Source source{ content, w.providerId, w.activityId, draft, &s.scrolls, drag, &s.keptText };
    Content asked;
    const Dialog* dialog = dialogOver(s, w.activityId);
    if (dialog) {
        asked = dialogContent(*dialog);
        source.content = &asked;
        scene.label = dialog->title;
    }
    float bar = t.windowTitleBarHeight;
    Vec2 card{ w.size.x, std::max(0.0f, w.size.y - bar) };
    // The same nesting as the island's card, so its content looks the same.
    float nestedRadius = std::max(0.0f, pillCornerRadius(card) - t.expandedPadding);
    std::size_t from = scene.elements.size();
    for (const SlotFrame& slot : cardSlots(*source.content, card)) placeSlot(slot, source, nestedRadius, now, scene.elements, measureText);
    moveDown(scene.elements, from, bar);

    if (const Toast* toast = dialog ? nullptr : toastOn(s, w.activityId))
        scene.toast = placeToast(*toast, source, scene.size, scene.size.y - t.expandedPadding, now, measureText);
    return scene;
}

} // namespace cup
