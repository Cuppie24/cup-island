#include "platform/element_visuals.h"

#include "core/tokens.h"
#include "platform/element_painting.h"
#include "platform/element_parts.h"

#include <algorithm>

namespace cup::platform {
namespace {

Vec2 sizeOf(const Placed& p) { return { p.frame.width, p.frame.height }; }

bool isRing(const Item& item)
{
    auto* progress = std::get_if<Progress>(&item);
    return progress && progress->shape == ProgressShape::Ring;
}

// Live data's colour: the content's accent, else the live colour (decision 010).
Color accentOf(const Placed& p) { return p.accent.value_or(tokens().liveColor); }

// Swimming text is sized to its string, so any change to it needs a new surface.
bool swims(const Item& item)
{
    auto* text = std::get_if<Text>(&item);
    return text && text->marquee;
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
        bool fresh = it == views_.end();
        if (fresh) views_.emplace(placed.key, create(placed, now));
        else fresh = update(it->second, placed, now);
        if (fresh && placed.key == hovered_) showHover(hovered_, true); // rebuilt under the pointer
    }

    // The GPU was reset mid-apply: the device is rebuilt, but every surface lost its pixels.
    if (deviceLost_)
        for (auto& [key, view] : views_)
            if (view.surface) paint(view);
}

void ElementVisuals::refresh()
{
    for (auto& [key, view] : views_) parent_.Children().Remove(view.visual);
    views_.clear();
}

ElementVisuals::View ElementVisuals::create(const Placed& placed, Ms now)
{
    const Tokens& t = tokens();
    View view;
    view.placed = placed;
    Vec2 size = sizeOf(placed), visualSize = size;

    if (isRing(placed.item)) {
        RingParts parts = createRing(compositor_, size, accentOf(placed));
        view.visual = parts.visual;
        view.arc = parts.arc;
    } else if (std::holds_alternative<Progress>(placed.item)) {
        ProgressParts parts = createProgress(compositor_, size, accentOf(placed));
        view.lead = progressOverhang();
        visualSize.x += 2 * view.lead;
        view.visual = parts.visual;
        view.fill = parts.fill;
        view.knob = parts.knob;
    } else if (std::holds_alternative<Waveform>(placed.item)) {
        WaveformParts parts = createWaveform(compositor_, size, accentOf(placed));
        view.visual = parts.visual;
        view.waveform = parts.visual;
        view.bars = parts.bars;
    } else if (std::holds_alternative<Button>(placed.item)) {
        view.surface = device_->createSurface(size);
        ButtonParts parts = createButton(compositor_, size, placed.cornerRadius, view.surface);
        view.visual = parts.visual;
        view.highlight = parts.highlight;
    } else {
        auto sprite = compositor_.CreateSpriteVisual();
        float width = swims(placed.item) ? measureText(device_->dwrite(), std::get<Text>(placed.item)) : 0;
        if (width > size.x) {
            // Too long: swim. The visual reaches left into the gap by the fade
            // width, so at rest the text starts exactly where static text would.
            view.marqueeWidth = width;
            view.lead = t.marqueeFadeWidth;
            visualSize.x += view.lead;
            view.surface = device_->createSurface({ view.lead + 2 * width + t.marqueeGap, size.y });
            sprite.Brush(createMarqueeBrush(compositor_, view.surface, visualSize, view.lead, width + t.marqueeGap));
        } else {
            view.surface = device_->createSurface(size);
            sprite.Brush(compositor_.CreateSurfaceBrush(view.surface));
        }
        view.visual = sprite;
    }
    view.visual.Offset({ placed.frame.x - view.lead, placed.frame.y, 0 });
    view.visual.Size(toFloat2(visualSize));
    view.visual.CenterPoint({ visualSize.x / 2, visualSize.y / 2, 0 });
    parent_.Children().InsertAtTop(view.visual);

    if (view.surface) paint(view);
    if (view.fill) armProgress(compositor_, view.fill, std::get<Progress>(placed.item).timeline, size.x, now);
    if (view.arc) armRing(compositor_, view.arc, std::get<Progress>(placed.item).timeline, now);
    if (!view.bars.empty()) animateBars(view);
    fadeIn(compositor_, view.visual);
    return view;
}

bool ElementVisuals::update(View& view, const Placed& placed, Ms now)
{
    bool resized = sizeOf(view.placed) != sizeOf(placed) || view.placed.cornerRadius != placed.cornerRadius;
    // A bar and a ring are both Progress, but different visuals; the accent is baked into brushes.
    bool retyped = view.placed.item.index() != placed.item.index() || isRing(view.placed.item) != isRing(placed.item) ||
                   view.placed.accent != placed.accent;
    bool moved = view.placed.frame.x != placed.frame.x || view.placed.frame.y != placed.frame.y;
    bool changed = !(view.placed.item == placed.item);
    if (resized || retyped || (changed && (swims(view.placed.item) || swims(placed.item)))) {
        // A different kind of visual, a new size (tokens.json edits) or a new swimming string: rebuild it.
        parent_.Children().Remove(view.visual);
        view = create(placed, now);
        return true;
    }
    view.placed = placed;
    if (moved) view.visual.Offset({ placed.frame.x - view.lead, placed.frame.y, 0 });
    if (!changed) return false;
    if (view.surface) paint(view);
    // While scrubbing, the bar belongs to the pointer; endScrub() re-arms it.
    if (view.fill && !view.scrubbing) armProgress(compositor_, view.fill, std::get<Progress>(placed.item).timeline, placed.frame.width, now);
    if (view.arc) armRing(compositor_, view.arc, std::get<Progress>(placed.item).timeline, now);
    if (!view.bars.empty()) animateBars(view);
    return false;
}

void ElementVisuals::paint(View& view)
{
    const Tokens& t = tokens();
    Vec2 size = sizeOf(view.placed);
    IDWriteFactory* dw = device_->dwrite();
    const Item& item = view.placed.item;
    bool drawn = device_->draw(view.surface, [&](ID2D1DeviceContext* dc) {
        if (auto* text = std::get_if<Text>(&item)) {
            if (view.marqueeWidth > 0) paintMarquee(dc, dw, *text, view.marqueeWidth, view.lead, t.marqueeGap, size.y);
            else paintText(dc, dw, *text, size);
        } else if (auto* icon = std::get_if<Icon>(&item)) {
            paintGlyph(dc, dw, icon->glyph, t.iconSize, size, t.iconColor);
        } else if (auto* button = std::get_if<Button>(&item)) {
            bool toggledOff = button->on && !*button->on;
            Color color = !button->enabled ? t.buttonDisabledColor : toggledOff ? t.buttonOffColor : t.buttonColor;
            if (!button->label.empty()) paintLabel(dc, dw, button->label, size, color);
            else paintGlyph(dc, dw, button->glyph, button->small ? t.iconSize : t.buttonGlyphSize, size, color);
            if (button->on && *button->on) paintToggleDot(dc, size, t.buttonGlyphSize);
        } else if (auto* image = std::get_if<Image>(&item)) {
            paintImage(dc, image->data.get(), size, view.placed.cornerRadius);
        } else if (auto* input = std::get_if<TextInput>(&item)) {
            paintInput(dc, dw, *input, size, view.placed.cornerRadius); // nested, like a button's hover shape
        } else if (auto* steps = std::get_if<Steps>(&item)) {
            paintSteps(dc, *steps, size, accentOf(view.placed));
        }
    });
    if (!drawn) deviceLost_ = true;
}

void ElementVisuals::animateBars(View& view)
{
    WaveformParts parts{ view.waveform, view.bars };
    animateWaveform(compositor_, parts, view.placed.frame.height, std::get<Waveform>(view.placed.item).active);
}

void ElementVisuals::fadeOutAndRemove(wuc::Visual const& visual)
{
    auto batch = compositor_.CreateScopedBatch(wuc::CompositionBatchTypes::Animation);
    fadeTo(compositor_, visual, 0.0f, tokens().contentFadeDuration / 2);
    batch.End();
    batch.Completed([parent = parent_, visual](auto&&, auto&&) { parent.Children().Remove(visual); });
}

// --- Pointer feedback ---------------------------------------------------------

void ElementVisuals::setHovered(const std::string& key)
{
    if (key == hovered_) return;
    showHover(hovered_, false);
    hovered_ = key;
    showHover(hovered_, true);
}

void ElementVisuals::showHover(const std::string& key, bool on)
{
    auto it = views_.find(key);
    if (it == views_.end()) return;
    View& view = it->second;
    if (view.highlight) fadeTo(compositor_, view.highlight, on ? 1.0f : 0.0f, tokens().hoverFadeDuration);
    if (view.knob && !view.scrubbing) showKnob(compositor_, view.knob, on);
}

void ElementVisuals::press(const std::string& key)
{
    auto it = views_.find(key);
    if (it == views_.end()) return;
    const Tokens& t = tokens();
    if (it->second.highlight) fadeTo(compositor_, it->second.highlight, 1.0f, t.hoverFadeDuration);
    playScale(compositor_, it->second.visual, t.buttonPressScale, t.pressDownDuration, t.pressDownEase);
}

void ElementVisuals::release(const std::string& key)
{
    auto it = views_.find(key);
    if (it == views_.end()) return;
    const Tokens& t = tokens();
    if (it->second.highlight && key != hovered_) fadeTo(compositor_, it->second.highlight, 0.0f, t.hoverFadeDuration);
    playScale(compositor_, it->second.visual, 1.0f, t.pressReleaseDuration, t.pressReleaseEase);
}

// The bar never squashes on press: it's a slider, so it follows the pointer instead.
void ElementVisuals::scrub(const std::string& key, float fraction)
{
    auto it = views_.find(key);
    if (it == views_.end() || !it->second.fill) return;
    View& view = it->second;
    if (!view.scrubbing) showKnob(compositor_, view.knob, true);
    view.scrubbing = true;
    holdProgress(view.fill, std::clamp(fraction, 0.0f, 1.0f) * view.placed.frame.width);
}

void ElementVisuals::endScrub(const std::string& key, std::optional<float> seekTo, Ms now)
{
    auto it = views_.find(key);
    if (it == views_.end() || !it->second.fill) return;
    View& view = it->second;
    view.scrubbing = false;
    // Play on from the release point right away; the provider's own update
    // (if the seek landed elsewhere) re-arms the bar when it arrives.
    Timeline timeline = std::get<Progress>(view.placed.item).timeline;
    if (seekTo) {
        timeline.position = Ms{ static_cast<long long>(static_cast<double>(timeline.duration.count()) * *seekTo) };
        timeline.anchoredAt = now;
    }
    armProgress(compositor_, view.fill, timeline, view.placed.frame.width, now);
    showKnob(compositor_, view.knob, key == hovered_);
}

} // namespace cup::platform
