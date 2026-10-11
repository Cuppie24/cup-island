#include "platform/element_visuals.h"

#include "core/tokens.h"
#include "platform/element_painting.h"

#include <algorithm>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;
using winrt::Windows::UI::Color;

Vec2 sizeOf(const Placed& p) { return { p.frame.width, p.frame.height }; }

// Composition rejects zero-length animations.
Ms atLeast1(Ms d) { return std::max(d, Ms{ 1 }); }

CompositionEasingFunction ease(Compositor const& c)
{
    Bezier curve = tokens().morphEase;
    return c.CreateCubicBezierEasingFunction(toFloat2(curve.c1), toFloat2(curve.c2));
}

void fadeIn(Compositor const& c, Visual const& visual)
{
    auto anim = c.CreateScalarKeyFrameAnimation();
    anim.InsertKeyFrame(0.0f, 0.0f);
    anim.InsertKeyFrame(1.0f, 1.0f, ease(c));
    anim.Duration(atLeast1(tokens().contentFadeDuration));
    anim.DelayTime(tokens().contentFadeDelay); // let the pill start morphing first
    anim.DelayBehavior(AnimationDelayBehavior::SetInitialValueBeforeDelay);
    visual.StartAnimation(L"Opacity", anim);
}

void playScale(Compositor const& c, Visual const& visual, float scale, Ms duration, Bezier curve)
{
    auto anim = c.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, { scale, scale, 1.0f }, c.CreateCubicBezierEasingFunction(toFloat2(curve.c1), toFloat2(curve.c2)));
    anim.Duration(atLeast1(duration));
    visual.StartAnimation(L"Scale", anim);
}

ShapeVisual createProgress(Compositor const& c, Vec2 size, CompositionRoundedRectangleGeometry& fill)
{
    float h = tokens().progressHeight, y = (size.y - h) / 2;
    auto bar = [&](float width, uint8_t alpha) {
        auto geometry = c.CreateRoundedRectangleGeometry();
        geometry.Size({ width, h });
        geometry.Offset({ 0, y });
        geometry.CornerRadius({ h / 2, h / 2 });
        auto shape = c.CreateSpriteShape(geometry);
        shape.FillBrush(c.CreateColorBrush(Color{ alpha, 255, 255, 255 }));
        return std::pair{ geometry, shape };
    };
    auto visual = c.CreateShapeVisual();
    visual.Shapes().Append(bar(size.x, 60).second);
    auto [fillGeometry, fillShape] = bar(0, 230);
    visual.Shapes().Append(fillShape);
    fill = fillGeometry;
    return visual;
}

ContainerVisual createWaveform(Compositor const& c, Vec2 size, std::vector<SpriteVisual>& bars)
{
    constexpr int kBars = 4;
    constexpr float kBarWidth = 3, kGap = 2.5f;
    float height = size.y * 0.8f, x = (size.x - (kBars * kBarWidth + (kBars - 1) * kGap)) / 2;
    auto visual = c.CreateContainerVisual();
    for (int i = 0; i < kBars; ++i) {
        auto bar = c.CreateSpriteVisual();
        bar.Brush(c.CreateColorBrush(Color{ 230, 255, 255, 255 }));
        bar.Size({ kBarWidth, height });
        bar.Offset({ x + static_cast<float>(i) * (kBarWidth + kGap), (size.y - height) / 2, 0 });
        bar.CenterPoint({ kBarWidth / 2, height / 2, 0 });
        visual.Children().InsertAtTop(bar);
        bars.push_back(bar);
    }
    return visual;
}

} // namespace

void ElementVisuals::init(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent)
{
    compositor_ = compositor;
    device_ = &device;
    parent_ = parent;
}

void ElementVisuals::apply(const std::vector<Placed>& elements, Ms now)
{
    deviceLost_ = false;

    for (auto it = views_.begin(); it != views_.end();) {
        bool stillShown = std::any_of(elements.begin(), elements.end(),
                                      [&](const Placed& p) { return p.key == it->first; });
        if (stillShown) { ++it; continue; }
        fadeOutAndRemove(it->second.visual);
        it = views_.erase(it);
    }

    for (const Placed& placed : elements) {
        auto it = views_.find(placed.key);
        if (it != views_.end()) update(it->second, placed, now);
        else views_.emplace(placed.key, create(placed, now));
    }

    // The GPU was reset mid-apply: the device is rebuilt, but every surface lost its pixels.
    if (deviceLost_)
        for (auto& [key, view] : views_)
            if (view.surface) paint(view);
}

ElementVisuals::View ElementVisuals::create(const Placed& placed, Ms now)
{
    View view;
    view.placed = placed;
    Vec2 size = sizeOf(placed);

    if (std::holds_alternative<Progress>(placed.item)) {
        view.visual = createProgress(compositor_, size, view.fill);
    } else if (std::holds_alternative<Waveform>(placed.item)) {
        view.visual = createWaveform(compositor_, size, view.bars);
    } else {
        auto sprite = compositor_.CreateSpriteVisual();
        view.surface = device_->createSurface(size);
        sprite.Brush(compositor_.CreateSurfaceBrush(view.surface));
        view.visual = sprite;
    }
    view.visual.Offset({ placed.frame.x, placed.frame.y, 0 });
    view.visual.Size(toFloat2(size));
    view.visual.CenterPoint({ size.x / 2, size.y / 2, 0 });
    parent_.Children().InsertAtTop(view.visual);

    if (view.surface) paint(view);
    if (view.fill) animateProgress(view, now);
    if (!view.bars.empty()) animateWaveform(view);
    fadeIn(compositor_, view.visual);
    return view;
}

void ElementVisuals::update(View& view, const Placed& placed, Ms now)
{
    bool resized = sizeOf(view.placed) != sizeOf(placed);
    bool retyped = view.placed.item.index() != placed.item.index(); // e.g. waveform -> pause icon
    bool moved = view.placed.frame.x != placed.frame.x || view.placed.frame.y != placed.frame.y;
    bool changed = !(view.placed.item == placed.item);
    if (resized || retyped) {
        // A different kind of visual, or a new size (tokens.json edits): rebuild it.
        parent_.Children().Remove(view.visual);
        view = create(placed, now);
        return;
    }
    view.placed = placed;
    if (moved) view.visual.Offset({ placed.frame.x, placed.frame.y, 0 });
    if (!changed) return;
    if (view.surface) paint(view);
    if (view.fill) animateProgress(view, now);
    if (!view.bars.empty()) animateWaveform(view);
}

void ElementVisuals::paint(View& view)
{
    const Tokens& t = tokens();
    Vec2 size = sizeOf(view.placed);
    IDWriteFactory* dw = device_->dwrite();
    const Item& item = view.placed.item;
    bool drawn = device_->draw(view.surface, [&](ID2D1DeviceContext* dc) {
        if (auto* text = std::get_if<Text>(&item)) paintText(dc, dw, *text, size);
        else if (auto* icon = std::get_if<Icon>(&item)) paintGlyph(dc, dw, icon->glyph, t.iconSize, size, 0.9f);
        else if (auto* button = std::get_if<Button>(&item))
            paintGlyph(dc, dw, button->glyph, t.buttonSize * 0.5f, size, button->enabled ? 0.95f : 0.3f);
        else if (auto* image = std::get_if<Image>(&item))
            paintImage(dc, image->data.get(), size, std::min(t.artCornerRadius, size.y / 4));
    });
    if (!drawn) deviceLost_ = true;
}

// The fill grows from where playback is now to full width over the remaining
// time, entirely on the compositor. Re-armed only when the timeline changes.
void ElementVisuals::animateProgress(View& view, Ms now)
{
    const Timeline& timeline = std::get<Progress>(view.placed.item).timeline;
    float width = view.placed.frame.width, height = tokens().progressHeight;
    float from = timeline.fractionAt(now) * width;

    view.fill.StopAnimation(L"Size");
    view.fill.Size({ from, height });
    if (!timeline.playing || timeline.rate <= 0 || timeline.duration <= Ms{ 0 }) return;

    Ms left = timeline.duration - timeline.positionAt(now);
    Ms wall{ static_cast<long long>(static_cast<double>(left.count()) / timeline.rate) };
    if (wall <= Ms{ 0 }) return;
    auto anim = compositor_.CreateVector2KeyFrameAnimation();
    anim.InsertKeyFrame(0.0f, { from, height });
    anim.InsertKeyFrame(1.0f, { width, height }, compositor_.CreateLinearEasingFunction());
    anim.Duration(std::min(wall, Ms{ 24LL * 3600 * 1000 })); // Composition caps durations
    view.fill.StartAnimation(L"Size", anim);
}

void ElementVisuals::animateWaveform(View& view)
{
    bool active = std::get<Waveform>(view.placed.item).active;
    for (size_t i = 0; i < view.bars.size(); ++i) {
        auto& bar = view.bars[i];
        bar.StopAnimation(L"Scale.Y");
        bar.Scale({ 1.0f, 0.35f, 1.0f });
        if (!active) continue;
        auto anim = compositor_.CreateScalarKeyFrameAnimation();
        anim.InsertKeyFrame(0.0f, 0.35f);
        anim.InsertKeyFrame(0.5f, 1.0f);
        anim.InsertKeyFrame(1.0f, 0.35f);
        anim.Duration(Ms{ 520 + 110 * static_cast<long long>(i) }); // different periods: looks organic
        anim.IterationBehavior(AnimationIterationBehavior::Forever);
        bar.StartAnimation(L"Scale.Y", anim);
    }
}

void ElementVisuals::fadeOutAndRemove(wuc::Visual const& visual)
{
    auto batch = compositor_.CreateScopedBatch(CompositionBatchTypes::Animation);
    auto anim = compositor_.CreateScalarKeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, 0.0f);
    anim.Duration(atLeast1(tokens().contentFadeDuration / 2));
    visual.StartAnimation(L"Opacity", anim);
    batch.End();
    batch.Completed([parent = parent_, visual](auto&&, auto&&) { parent.Children().Remove(visual); });
}

void ElementVisuals::press(const std::string& key)
{
    if (auto it = views_.find(key); it != views_.end())
        playScale(compositor_, it->second.visual, 0.85f, tokens().pressDownDuration, tokens().pressDownEase);
}

void ElementVisuals::release(const std::string& key)
{
    if (auto it = views_.find(key); it != views_.end())
        playScale(compositor_, it->second.visual, 1.0f, tokens().pressReleaseDuration, tokens().pressReleaseEase);
}

} // namespace cup::platform
