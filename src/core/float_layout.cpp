#include "core/float_layout.h"

#include "core/dialog.h"
#include "core/slot_layout.h"
#include "core/toast.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// The ✕ sits on the card's top-right corner, half outside it, so it covers
// none of the card's own controls (the corner button of a Standard card).
void placeClose(const FloatingCard& f, Vec2 size, std::vector<Placed>& out)
{
    float side = tokens().floatCloseSize;
    Rect frame{ size.x - side / 2, -side / 2, side, side };
    out.push_back({ .key = kCloseFloatKey, .item = Button{ .glyph = Glyph::Close, .actionId = kCloseFloat, .compact = true, .secondary = true },
                    .frame = frame, .hitFrame = frame, .cornerRadius = side / 2, .providerId = f.providerId, .activityId = f.activityId });
}

} // namespace

Scene floatScene(const IslandState& s, const FloatingCard& f, Ms now, const MeasureText& measure)
{
    const Tokens& t = tokens();
    MeasureText measureText = measure ? measure : MeasureText{ estimateTextWidth };
    Scene scene;
    scene.now = now;
    scene.mode = Mode::Expanded;
    scene.floating = true;
    scene.activityId = f.activityId;
    const Content* content = cardContent(s, f.activityId);
    if (!content) return scene;
    scene.label = content->label;

    // What the island's card would show; a question takes its place, as there (decision 020).
    const Draft* draft = s.draft && s.draft->activityId == f.activityId ? &*s.draft : nullptr;
    const ListDrag* drag = s.listDrag && s.listDrag->activityId == f.activityId ? &*s.listDrag : nullptr;
    Source source{ content, f.providerId, f.activityId, draft, &s.scrolls, drag, &s.keptText };
    Content asked;
    const Dialog* dialog = dialogOver(s, f.activityId);
    if (dialog) {
        asked = dialogContent(*dialog);
        source.content = &asked;
        scene.label = dialog->title;
    }

    // Fits its content, like the island's cards (decision 016), unless resized.
    float width = content->floating ? floatWidth(content->floating->width) : t.floatMediumWidth;
    if (f.size) scene.size = *f.size;
    else if (stacked(*source.content)) scene.size = { width, centredCardHeight(source, { width, 0 }, t.floatMaxHeight, measureText) };
    else scene.size = { width, std::min(t.expandedSize.y, t.floatMaxHeight) };

    float nestedRadius = std::max(0.0f, pillCornerRadius(scene.size) - t.expandedPadding); // as on the island's card
    for (const SlotFrame& slot : cardSlots(*source.content, scene.size)) placeSlot(slot, source, nestedRadius, now, scene.elements, measureText);
    if (const Toast* toast = dialog ? nullptr : toastOn(s, f.activityId))
        scene.toast = placeToast(*toast, source, scene.size, scene.size.y - t.expandedPadding, now, measureText);
    placeClose(f, scene.size, scene.elements);
    return scene;
}

} // namespace cup
