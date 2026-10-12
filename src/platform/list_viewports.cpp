#include "platform/list_viewports.h"

#include "core/tokens.h"
#include "platform/blob_shape.h"

#include <algorithm>
#include <cmath>

namespace cup::platform {
namespace {

// Offset eases from wherever it is now to each new value: a new wheel step
// mid-glide carries on from there (decision 029).
wuc::ImplicitAnimationCollection offsetEase(wuc::Compositor const& c)
{
    auto offset = c.CreateVector3KeyFrameAnimation();
    offset.InsertExpressionKeyFrame(1.0f, L"this.FinalValue", easingFor(c, tokens().morphEase));
    offset.Duration(tokens().scrollDuration);
    offset.Target(L"Offset");
    auto collection = c.CreateImplicitAnimationCollection();
    collection.Insert(L"Offset", offset);
    return collection;
}

wfn::float3 pixels(float x, float y) { return { std::round(x), std::round(y), 0.0f }; }

} // namespace

void ListViewports::init(wuc::Compositor const& compositor, wuc::ContainerVisual const& parent)
{
    compositor_ = compositor;
    parent_ = parent;
}

wuc::ContainerVisual ListViewports::parentFor(const Placed& placed)
{
    if (!placed.list) return parent_;
    const InList& in = *placed.list;
    // Built lazily: tokens.json may change the duration, and refresh() clears these.
    if (!scrollEase_) scrollEase_ = offsetEase(compositor_);
    if (!glide_) glide_ = offsetEase(compositor_);
    List& list = lists_[keyOf(placed)];
    if (!list.clip) {
        list.clip = compositor_.CreateContainerVisual();
        list.clip.Clip(compositor_.CreateInsetClip());
        list.scroller = compositor_.CreateContainerVisual();
        list.clip.Children().InsertAtTop(list.scroller);
        parent_.Children().InsertAtTop(list.clip);
        // Placed where it starts before it eases: a new list doesn't glide in from 0.
        list.viewport = in.viewport;
        list.offset = in.offset;
        list.clip.Offset(pixels(in.viewport.x, in.viewport.y));
        list.clip.Size({ in.viewport.width, in.viewport.height });
        list.scroller.Offset(pixels(0, -in.offset));
        list.scroller.ImplicitAnimations(scrollEase_);
        return list.scroller;
    }
    if (!(list.viewport == in.viewport)) { // the card grew or moved: the window goes with it, at once
        list.viewport = in.viewport;
        list.clip.Offset(pixels(in.viewport.x, in.viewport.y));
        list.clip.Size({ in.viewport.width, in.viewport.height });
    }
    if (list.offset != in.offset) {
        list.offset = in.offset;
        list.scroller.Offset(pixels(0, -in.offset)); // eases (scrollEase_)
    }
    return list.scroller;
}

Vec2 ListViewports::positionOf(const Placed& placed) const
{
    if (!placed.list) return { placed.frame.x, placed.frame.y };
    const InList& in = *placed.list;
    return { placed.frame.x - in.viewport.x, placed.frame.y + in.offset - in.viewport.y };
}

void ListViewports::setGliding(wuc::Visual const& visual, bool glide)
{
    if (!glide_) glide_ = offsetEase(compositor_);
    visual.ImplicitAnimations(glide ? glide_ : nullptr);
}

void ListViewports::prune(const std::vector<Placed>& elements)
{
    for (auto it = lists_.begin(); it != lists_.end();) {
        bool used = std::any_of(elements.begin(), elements.end(), [&](const Placed& p) { return p.list && keyOf(p) == it->first; });
        if (used) { ++it; continue; }
        parent_.Children().Remove(it->second.clip); // its elements already faded out with their views
        it = lists_.erase(it);
    }
}

void ListViewports::clear()
{
    for (auto& [key, list] : lists_) parent_.Children().Remove(list.clip);
    lists_.clear();
    scrollEase_ = nullptr;
    glide_ = nullptr;
}

} // namespace cup::platform
