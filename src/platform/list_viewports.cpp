#include "platform/list_viewports.h"

#include "core/tokens.h"
#include "platform/blob_shape.h"
#include "platform/effects.h"
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

// CreateVisualSurface and CreateRadialGradientBrush are Windows 10 1903+ (decision 015): before
// them, a list under buttons is drawn as any other, its words passing under them.
bool canCutHoles()
{
    using winrt::Windows::Foundation::Metadata::ApiInformation;
    static const bool can = ApiInformation::IsMethodPresent(L"Windows.UI.Composition.Compositor", L"CreateVisualSurface") &&
                            ApiInformation::IsMethodPresent(L"Windows.UI.Composition.Compositor", L"CreateRadialGradientBrush");
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
    if (!list.clip || in.holes.size() != list.asked) {
        list.asked = in.holes.size();
        buildWindow(list, in);
    }
    // The card grew or moved: the window goes with it, at once.
    if (!(list.viewport == in.viewport) || list.holeFrames != in.holes || list.holeFade != in.holeFade) placeWindow(list, in);
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
    list.holes.clear();
    if (list.asked == 0 || !holedWindow(list, list.asked)) {
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

bool ListViewports::holedWindow(List& list, std::size_t holes)
{
    if (!canCutHoles()) {
        static bool told = false;
        if (!told) logLine("a list's holes: no snapshots here (before Windows 10 1903): its words pass under the buttons");
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
        // Each hole source-in the ones before, the list last: its colours, in the holes' alpha
        // multiplied (as swimming text's edges, element_parts). One effect, nothing nested (decision 100).
        std::vector<IGraphicsEffectSource> layers;
        std::vector<wuc::CompositionRadialGradientBrush> cut;
        for (std::size_t i = 0; i < holes; ++i) {
            layers.push_back(wuc::CompositionEffectSourceParameter(L"Hole" + winrt::to_hstring(static_cast<std::uint32_t>(i))));
            auto hole = compositor_.CreateRadialGradientBrush(); // placed and given its stops by placeWindow
            hole.MappingMode(wuc::CompositionMappingMode::Absolute);
            cut.push_back(hole);
        }
        layers.push_back(wuc::CompositionEffectSourceParameter(L"List"));
        auto brush = compositor_.CreateEffectFactory(composite(L"Holes", D2D1_COMPOSITE_MODE_SOURCE_IN, std::move(layers))).CreateBrush();
        for (std::size_t i = 0; i < holes; ++i) brush.SetSourceParameter(L"Hole" + winrt::to_hstring(static_cast<std::uint32_t>(i)), cut[i]);
        brush.SetSourceParameter(L"List", picture);
        auto sprite = compositor_.CreateSpriteVisual();
        sprite.Brush(brush);
        list.clip = sprite;
        list.hidden = hidden;
        list.root = root;
        list.snapshot = snapshot;
        list.holes = std::move(cut);
        return true;
    } catch (winrt::hresult_error const& e) {
        logLine("a list's holes: refused (0x%08lx): its words pass under the buttons", static_cast<unsigned long>(e.code()));
        if (auto parent = list.scroller.Parent()) parent.Children().Remove(list.scroller);
        return false;
    }
}

void ListViewports::placeWindow(List& list, const InList& in)
{
    list.viewport = in.viewport;
    list.holeFrames = in.holes;
    list.holeFade = in.holeFade;
    wfn::float2 size{ in.viewport.width, in.viewport.height };
    list.clip.Offset(pixels(in.viewport.x, in.viewport.y));
    list.clip.Size(size);
    if (!list.snapshot) return;
    list.root.Size(size);
    list.snapshot.SourceSize(size);
    for (std::size_t i = 0; i < list.holes.size() && i < in.holes.size(); ++i) {
        // Round the button: clear out to its edge, then eased back to whole over holeFade
        // (a straight ramp shows where it starts and ends). An opacity mask: only alpha matters.
        const Rect& b = in.holes[i];
        float inner = std::min(b.width, b.height) / 2, outer = inner + std::max(in.holeFade, 1.0f);
        wuc::CompositionRadialGradientBrush& hole = list.holes[i];
        hole.EllipseCenter({ b.x + b.width / 2, b.y + b.height / 2 });
        hole.EllipseRadius({ outer, outer });
        hole.ColorStops().Clear();
        float start = inner / outer;
        hole.ColorStops().Append(compositor_.CreateColorGradientStop(0.0f, winrt::Windows::UI::Color{ 0, 255, 255, 255 }));
        for (float at : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) {
            float eased = at * at * (3 - 2 * at);
            auto alpha = static_cast<std::uint8_t>(std::lround(255 * eased));
            hole.ColorStops().Append(compositor_.CreateColorGradientStop(start + (1 - start) * at, winrt::Windows::UI::Color{ alpha, 255, 255, 255 }));
        }
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
