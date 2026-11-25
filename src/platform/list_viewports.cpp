#include "platform/list_viewports.h"

#include "core/tokens.h"
#include "platform/blob_shape.h"
#include "platform/log.h"

#include <winrt/Windows.Foundation.Metadata.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cup::platform {
namespace {

// Offset eases from wherever it is now to each new value: a new wheel step
// mid-glide carries on from there (decision 029).
wuc::ImplicitAnimationCollection offsetEase(wuc::Compositor const& c, Ms duration)
{
    auto offset = c.CreateVector3KeyFrameAnimation();
    offset.InsertExpressionKeyFrame(1.0f, L"this.FinalValue", easingFor(c, tokens().morphEase));
    offset.Duration(std::max(duration, Ms{ 1 }));
    offset.Target(L"Offset");
    auto collection = c.CreateImplicitAnimationCollection();
    collection.Insert(L"Offset", offset);
    return collection;
}

wfn::float3 pixels(float x, float y) { return { std::round(x), std::round(y), 0.0f }; }

// CreateVisualSurface is Windows 10 1903+ (decision 015): before it, a faded list is cut where its fade would start.
bool canSnapshot()
{
    using winrt::Windows::Foundation::Metadata::ApiInformation;
    static const bool can = ApiInformation::IsMethodPresent(L"Windows.UI.Composition.Compositor", L"CreateVisualSurface");
    return can;
}

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
    if (!scrollEase_) scrollEase_ = offsetEase(compositor_, tokens().scrollDuration);
    if (!followEase_) followEase_ = offsetEase(compositor_, tokens().followScrollDuration);
    if (!glide_) glide_ = offsetEase(compositor_, tokens().scrollDuration);
    List& list = lists_[keyOf(placed)];
    if (!list.scroller) {
        list.scroller = compositor_.CreateContainerVisual();
        // Placed where it starts before it eases: a new list doesn't glide in from 0.
        list.offset = in.offset;
        list.scroller.Offset(pixels(0, -in.offset));
        list.scroller.ImplicitAnimations(scrollEase_);
    }
    bool faded = in.hideTop > 0 || in.fadeTop > 0;
    if (!list.clip || faded != list.faded) {
        list.faded = faded;
        buildWindow(list, in);
    }
    // The card grew or moved: the window goes with it, at once.
    if (!(list.viewport == in.viewport) || list.hideTop != in.hideTop || list.fadeTop != in.fadeTop) placeWindow(list, in);
    if (list.offset != in.offset) {
        list.offset = in.offset;
        if (in.carrying) { // a row carried: at once, or it would lag behind the row it carries (decision 071)
            list.scroller.ImplicitAnimations(wuc::ImplicitAnimationCollection{ nullptr });
            list.scroller.StopAnimation(L"Offset");
            list.scroller.Offset(pixels(0, -in.offset));
            list.scroller.ImplicitAnimations(scrollEase_);
        } else {
            // Following a row that moves on (InList::following, decision 180): a slow glide; else the wheel's quick one.
            list.scroller.ImplicitAnimations(in.following ? followEase_ : scrollEase_);
            list.scroller.Offset(pixels(0, -in.offset)); // eases
        }
    }
    return list.scroller;
}

void ListViewports::buildWindow(List& list, const InList& in)
{
    wuc::ContainerVisual old = list.clip, oldHidden = list.hidden;
    if (list.root) list.root.Children().Remove(list.scroller); // a visual has one parent
    else if (old) old.Children().Remove(list.scroller);
    list.hidden = nullptr;
    list.root = nullptr;
    list.snapshot = nullptr;
    list.mask = nullptr;
    if (!list.faded || !fadedWindow(list)) {
        list.clip = compositor_.CreateContainerVisual();
        list.clip.Clip(compositor_.CreateInsetClip());
        list.clip.Children().InsertAtTop(list.scroller);
    }
    // In the old window's place among its siblings, so what was over it (the buttons) still is.
    if (old) {
        parent_.Children().InsertAbove(list.clip, old);
        parent_.Children().Remove(old);
    } else {
        parent_.Children().InsertAtTop(list.clip);
    }
    if (oldHidden) parent_.Children().Remove(oldHidden);
    if (list.hidden) parent_.Children().InsertBelow(list.hidden, list.clip);
    placeWindow(list, in);
}

bool ListViewports::fadedWindow(List& list)
{
    if (!canSnapshot()) {
        static bool told = false;
        if (!told) logLine("a list's fade: no snapshots here (before Windows 10 1903): cut where it would fade");
        told = true;
        return false;
    }
    try {
        // The snapshot draws its source as its own root, without its parents' opacity (decision 097):
        // the scroller's own root is in the tree, unseen, and only its snapshot shows. The root, not
        // the scroller: a source's own Offset is ignored, and the scroller's is the scroll.
        auto hidden = compositor_.CreateContainerVisual();
        hidden.Opacity(0.0f);
        auto root = compositor_.CreateContainerVisual();
        root.Children().InsertAtTop(list.scroller);
        hidden.Children().InsertAtTop(root);
        auto snapshot = compositor_.CreateVisualSurface(); // live, on the compositor: the scroll's glide included
        snapshot.SourceVisual(root);
        auto picture = compositor_.CreateSurfaceBrush(snapshot);
        picture.Stretch(wuc::CompositionStretch::None); // pixel for pixel, pinned top-left: the text stays sharp
        picture.HorizontalAlignmentRatio(0.0f);
        picture.VerticalAlignmentRatio(0.0f);
        // Clear above StartPoint, whole below EndPoint (placeWindow sets both), eased between:
        // a straight ramp shows where it starts and ends. An opacity mask: only alpha matters.
        auto mask = compositor_.CreateLinearGradientBrush();
        mask.MappingMode(wuc::CompositionMappingMode::Absolute);
        for (float at : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) {
            float eased = at * at * (3 - 2 * at);
            auto alpha = static_cast<std::uint8_t>(std::lround(255 * eased));
            mask.ColorStops().Append(compositor_.CreateColorGradientStop(at, winrt::Windows::UI::Color{ alpha, 255, 255, 255 }));
        }
        auto masked = compositor_.CreateMaskBrush();
        masked.Source(picture);
        masked.Mask(mask);
        auto sprite = compositor_.CreateSpriteVisual();
        sprite.Brush(masked);
        list.clip = sprite;
        list.hidden = hidden;
        list.root = root;
        list.snapshot = snapshot;
        list.mask = mask;
        return true;
    } catch (winrt::hresult_error const& e) {
        logLine("a list's fade: refused (0x%08lx): cut where it would fade", static_cast<unsigned long>(e.code()));
        if (auto parent = list.scroller.Parent()) parent.Children().Remove(list.scroller);
        return false;
    }
}

void ListViewports::placeWindow(List& list, const InList& in)
{
    list.viewport = in.viewport;
    list.hideTop = in.hideTop;
    list.fadeTop = in.fadeTop;
    wfn::float2 size{ in.viewport.width, in.viewport.height };
    list.clip.Offset(pixels(in.viewport.x, in.viewport.y));
    list.clip.Size(size);
    if (list.snapshot) {
        list.root.Size(size);
        list.snapshot.SourceSize(size);
        list.mask.StartPoint({ 0.0f, in.hideTop });
        list.mask.EndPoint({ 0.0f, in.hideTop + std::max(in.fadeTop, 1.0f) });
    } else if (auto cut = list.clip.Clip().try_as<wuc::InsetClip>()) {
        cut.TopInset(std::max(0.0f, in.hideTop)); // no snapshot: nothing under the buttons, but no fade either
    }
}

Vec2 ListViewports::positionOf(const Placed& placed)
{
    if (!placed.list) return { placed.frame.x, placed.frame.y };
    const InList& in = *placed.list;
    return { placed.frame.x - in.viewport.x, placed.frame.y + in.offset - in.viewport.y };
}

void ListViewports::setGliding(wuc::Visual const& visual, bool glide)
{
    if (!glide_) glide_ = offsetEase(compositor_, tokens().scrollDuration);
    visual.ImplicitAnimations(glide ? glide_ : nullptr);
}

void ListViewports::prune(const std::vector<Placed>& elements)
{
    for (auto it = lists_.begin(); it != lists_.end();) {
        bool used = std::any_of(elements.begin(), elements.end(), [&](const Placed& p) { return p.list && keyOf(p) == it->first; });
        if (used) { ++it; continue; }
        parent_.Children().Remove(it->second.clip); // its elements already faded out with their views
        if (it->second.hidden) parent_.Children().Remove(it->second.hidden);
        it = lists_.erase(it);
    }
}

void ListViewports::clear()
{
    for (auto& [key, list] : lists_) {
        parent_.Children().Remove(list.clip);
        if (list.hidden) parent_.Children().Remove(list.hidden);
    }
    lists_.clear();
    scrollEase_ = nullptr;
    followEase_ = nullptr;
    glide_ = nullptr;
}

} // namespace cup::platform
