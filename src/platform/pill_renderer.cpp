#include "platform/pill_renderer.h"

#include "core/tokens.h"

namespace cup::platform {

void PillRenderer::build(wuc::Compositor const& c, wuc::ContainerVisual const& root, const Scene& initial)
{
    device_.create(c);

    // Bodies below, content above: all bodies are the same black, so only
    // content would show which one is on top.
    auto bodies = c.CreateContainerVisual();
    auto contents = c.CreateContainerVisual();
    root.Children().InsertAtTop(bodies);
    root.Children().InsertAtTop(contents);

    blobs_[0].build(c, device_, bodies, contents, initial.size, initial.offset, false);
    blobs_[1].build(c, device_, bodies, contents, {}, initial.bubbleTuck, true);
    bridge_.build(c, bodies, tokens().canvas, blobs_[0], blobs_[1]);
    apply(initial);
}

void PillRenderer::refreshAnimations()
{
    for (Blob& blob : blobs_) blob.refreshAnimations();
    bridge_.refreshAnimations();
}

void PillRenderer::apply(const Scene& scene)
{
    // A shape stays with its activity when roles change (decision 006): the
    // dragged bubble grows into the pill while the old pill shrinks into the
    // bubble, a clicked bubble grows into the Expanded card, and so on.
    const std::string bubbleId = scene.bubble ? scene.bubble->activityId : "";
    bool pillNowInBubble = !bubbleId.empty() && pillBlob().owner == bubbleId;
    bool bubbleNowInPill = !scene.activityId.empty() && bubbleBlob().owner == scene.activityId &&
                           pillBlob().owner != scene.activityId;
    if (pillNowInBubble || bubbleNowInPill) pill_ = 1 - pill_;

    // The dragged shape follows the cursor in either role, fully rounded (a
    // floating drop, not a notch). Everything else sits at its frame.
    auto follows = [&](const Blob& blob) { return scene.drag && blob.owner == scene.drag->activityId; };
    auto centre = [&](const Blob& blob) { return follows(blob) ? std::optional<Vec2>(scene.drag->centre) : std::nullopt; };

    Blob& pill = pillBlob();
    pill.owner = scene.activityId;
    pill.place(scene.size, scene.offset);
    pill.setRound(follows(pill));
    pill.follow(centre(pill));
    pill.setContent(scene.elements, scene.now);

    Blob& bubble = bubbleBlob();
    if (scene.bubble) {
        const Rect& f = scene.bubble->frame;
        bubble.owner = bubbleId;
        bubble.place({ f.width, f.height }, { f.x, f.y });
        bubble.setRound(true);
        bubble.follow(centre(bubble));
        bubble.setContent(scene.bubble->elements, scene.now);
    } else {
        // No bubble: shrink into the pill (it buds back out of the same spot).
        bubble.owner.clear();
        bubble.place({}, scene.bubbleTuck);
        bubble.setRound(true);
        bubble.follow(std::nullopt);
        bubble.setContent({}, scene.now);
    }
}

} // namespace cup::platform
