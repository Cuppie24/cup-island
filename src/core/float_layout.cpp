#include "core/float_layout.h"

#include "core/dialog.h"
#include "core/slot_layout.h"
#include "core/toast.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// The ✕ at the right end of the title bar, as square in its corner as the
// bar is tall: inside the card, like a window's, and over none of the
// card's own controls (they're all under the bar).
void placeClose(const FloatingCard& f, Vec2 size, std::vector<Placed>& out)
{
    const Tokens& t = tokens();
    float side = t.floatCloseSize, inset = (t.floatTitleBarHeight - side) / 2;
    Rect frame{ size.x - inset - side, inset, side, side };
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

    // What the island's card would show; a question takes its place below, as there (decision 020).
    const Draft* draft = s.draft && s.draft->activityId == f.activityId ? &*s.draft : nullptr;
    const ListDrag* drag = s.listDrag && s.listDrag->activityId == f.activityId ? &*s.listDrag : nullptr;
    Source source{ content, f.providerId, f.activityId, draft, &s.scrolls, drag, &s.keptText };
    // Fits its content, like the island's cards (decision 016), unless resized:
    // the card's own, so a question over it changes nothing (decision 020).
    // Within its limits either way; the title bar is on top of the card's own height.
    float bar = t.floatTitleBarHeight;
    FloatLimits limits = floatLimits(content);
    float width = content->floating ? floatWidth(content->floating->width) : t.floatMediumWidth;
    if (f.size) scene.size = *f.size;
    else if (stacked(*content)) scene.size = { width, centredCardHeight(source, { width, 0 }, limits.most.y - bar, measureText) + bar };
    else scene.size = { width, t.expandedSize.y + bar };
    scene.size = clampSize(scene.size, limits);
    scene.titleBar = { 0, 0, scene.size.x, bar };

    Content asked;
    const Dialog* dialog = dialogOver(s, f.activityId);
    if (dialog) {
        asked = dialogContent(*dialog);
        source.content = &asked;
        scene.label = dialog->title;
    }

    // The card under the bar, laid out as the island's card is, with its radii.
    Vec2 card{ scene.size.x, std::max(0.0f, scene.size.y - bar) };
    float nestedRadius = std::max(0.0f, pillCornerRadius(card) - t.expandedPadding);
    std::size_t from = scene.elements.size();
    for (const SlotFrame& slot : cardSlots(*source.content, card)) placeSlot(slot, source, nestedRadius, now, scene.elements, measureText);
    moveDown(scene.elements, from, bar);
    if (const Toast* toast = dialog ? nullptr : toastOn(s, f.activityId))
        scene.toast = placeToast(*toast, source, scene.size, scene.size.y - t.expandedPadding, now, measureText);
    placeClose(f, scene.size, scene.elements);
    return scene;
}

} // namespace cup
