#include "platform/element_visuals.h"

#include "core/tokens.h"
#include "platform/element_painting.h"
#include "platform/element_parts.h"

#include <algorithm>
#include <cmath>

namespace cup::platform {
namespace {

// Pixel snapping (decision 017): sizes up to whole pixels, positions to the nearest one.
Vec2 pixelSize(Vec2 size) { return { std::ceil(size.x), std::ceil(size.y) }; }
wfn::float3 pixelOffset(float x, float y) { return { std::round(x), std::round(y), 0.0f }; }

Vec2 sizeOf(const Placed& p) { return { p.frame.width, p.frame.height }; }

bool isRing(const Item& item)
{
    auto* progress = std::get_if<Progress>(&item);
    return progress && progress->shape == ProgressShape::Ring;
}

// Live data's colour: the content's accent, else the live colour (decision 010).
Color accentOf(const Placed& p) { return p.accent.value_or(tokens().liveColor); }

// A list item's delete button: hidden until the pointer is on its row (decision 020).
bool revealsOnHover(const Item& item)
{
    auto* button = std::get_if<Button>(&item);
    return button && button->revealOnHover;
}

// "<slot>/<row>/<item>" -> "<slot>/<row>"
std::string rowOf(const std::string& key) { return key.substr(0, key.rfind('/')); }

// A focused text field keeps its background: you can see where you're typing,
// with the pointer anywhere (decision 016).
bool focusedField(const Item& item)
{
    auto* input = std::get_if<TextInput>(&item);
    return input && input->focused;
}

// Swimming text is sized to its string, so any change to it needs a new surface.
// Art with an action gets the hover overlay, so it's a different visual from plain art.
bool clickableArt(const Item& item)
{
    auto* image = std::get_if<Image>(&item);
    return image && !image->actionId.empty();
}

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
    // Whole pixels: frames are fractional (islandScale x display scale), and a
    // surface stretched into a fractional box is resampled, which blurs text.
    Vec2 size = pixelSize(sizeOf(placed)), visualSize = size;

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
    } else if (std::holds_alternative<Button>(placed.item) || std::holds_alternative<TextInput>(placed.item)) {
        // A text field is built like a button: its background is the hover shape, shown only under the pointer.
        bool field = std::holds_alternative<TextInput>(placed.item);
        view.surface = device_->createSurface(size);
        ButtonParts parts = createButton(compositor_, size, placed.cornerRadius, view.surface, field ? t.inputColor : t.buttonHoverColor);
        view.visual = parts.visual;
        view.highlight = parts.highlight;
    } else if (clickableArt(placed.item)) {
        view.surface = device_->createSurface(size);
        view.glyphSurface = device_->createSurface(size);
        view.art = createActionArt(compositor_, size, placed.cornerRadius, view.surface, view.glyphSurface);
        view.visual = view.art.visual;
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
    view.visual.Offset(pixelOffset(placed.frame.x - view.lead, placed.frame.y));
    view.visual.Size(toFloat2(pixelSize(visualSize)));
    view.visual.CenterPoint({ visualSize.x / 2, visualSize.y / 2, 0 });
    parent_.Children().InsertAtTop(view.visual);

    if (view.surface) paint(view);
    if (view.fill) armProgress(compositor_, view.fill, std::get<Progress>(placed.item).timeline, size.x, now);
    if (view.arc) armRing(compositor_, view.arc, std::get<Progress>(placed.item).timeline, now);
    if (!view.bars.empty()) animateBars(view);
    if (view.highlight && focusedField(placed.item)) view.highlight.Opacity(1.0f);
    if (revealsOnHover(placed.item) && rowOf(placed.key) != hoveredRow_) view.visual.Opacity(0.0f); // until its row is hovered
    else fadeIn(compositor_, view.visual);
    return view;
}

bool ElementVisuals::update(View& view, const Placed& placed, Ms now)
{
    bool resized = sizeOf(view.placed) != sizeOf(placed) || view.placed.cornerRadius != placed.cornerRadius;
    // A bar and a ring are both Progress, but different visuals; the accent is baked into brushes.
    bool retyped = view.placed.item.index() != placed.item.index() || isRing(view.placed.item) != isRing(placed.item) ||
                   clickableArt(view.placed.item) != clickableArt(placed.item) ||
                   view.placed.accent != placed.accent;
    bool moved = view.placed.frame.x != placed.frame.x || view.placed.frame.y != placed.frame.y;
    bool changed = !(view.placed.item == placed.item);
    if (resized || retyped || (changed && (swims(view.placed.item) || swims(placed.item)))) {
        // A different kind of visual, a new size (tokens.json edits) or a new swimming string: rebuild it.
        parent_.Children().Remove(view.visual);
        view = create(placed, now);
        return true;
    }
    bool wasFocused = focusedField(view.placed.item);
    view.placed = placed;
    if (moved) view.visual.Offset(pixelOffset(placed.frame.x - view.lead, placed.frame.y));
    if (!changed) return false;
    if (view.highlight && focusedField(placed.item) != wasFocused) // focus came or went: background follows (or hover keeps it)
        fadeTo(compositor_, view.highlight, focusedField(placed.item) || placed.key == hovered_ ? 1.0f : 0.0f, tokens().hoverFadeDuration);
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
            Color color = !button->enabled ? t.buttonDisabledColor : toggledOff ? t.buttonOffColor
                        : button->destructive         ? t.dangerColor
                                                      : t.buttonColor;
            if (!button->label.empty()) paintLabel(dc, dw, button->label, size, color);
            else paintGlyph(dc, dw, button->glyph, button->compact ? t.iconSize : t.buttonGlyphSize, size, color);
            if (button->on && *button->on) paintToggleDot(dc, size, t.buttonGlyphSize);
        } else if (auto* image = std::get_if<Image>(&item)) {
            paintImage(dc, image->data.get(), size, view.placed.cornerRadius);
        } else if (auto* input = std::get_if<TextInput>(&item)) {
            paintInput(dc, dw, *input, size, view.placed.cornerRadius); // nested, like a button's hover shape
        } else if (auto* steps = std::get_if<Steps>(&item)) {
            paintSteps(dc, *steps, size, accentOf(view.placed));
        }
    });
    if (drawn && view.glyphSurface)
        drawn = device_->draw(view.glyphSurface, [&](ID2D1DeviceContext* dc) {
            paintGlyph(dc, dw, Glyph::OpenApp, t.artHoverGlyphSize, size, t.artHoverGlyphColor);
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

void ElementVisuals::setHoveredRow(const std::string& row)
{
    if (row == hoveredRow_) return;
    hoveredRow_ = row;
    for (auto& [key, view] : views_)
        if (revealsOnHover(view.placed.item)) fadeTo(compositor_, view.visual, rowOf(key) == row ? 1.0f : 0.0f, tokens().hoverFadeDuration);
}

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
    if (view.highlight) fadeTo(compositor_, view.highlight, on || focusedField(view.placed.item) ? 1.0f : 0.0f, tokens().hoverFadeDuration);
    if (view.knob && !view.scrubbing) showKnob(compositor_, view.knob, on);
    if (view.art.overlay) showArtOverlay(compositor_, view.art, on);
}

void ElementVisuals::press(const std::string& key)
{
    auto it = views_.find(key);
    if (it == views_.end()) return;
    const Tokens& t = tokens();
    if (it->second.highlight) fadeTo(compositor_, it->second.highlight, 1.0f, t.hoverFadeDuration);
    if (it->second.art.overlay && key != hovered_) showArtOverlay(compositor_, it->second.art, true); // pressed without a hover (touch)
    if (std::holds_alternative<TextInput>(it->second.placed.item)) return; // a field is clicked into, not pressed: no squash
    // Art is big: the pill's gentle squash, not a button's deep one.
    float scale = std::holds_alternative<Image>(it->second.placed.item) ? t.pressScale : t.buttonPressScale;
    playScale(compositor_, it->second.visual, scale, t.pressDownDuration, t.pressDownEase);
}

void ElementVisuals::release(const std::string& key)
{
    auto it = views_.find(key);
    if (it == views_.end()) return;
    const Tokens& t = tokens();
    if (it->second.highlight && key != hovered_ && !focusedField(it->second.placed.item))
        fadeTo(compositor_, it->second.highlight, 0.0f, t.hoverFadeDuration);
    if (it->second.art.overlay && key != hovered_) showArtOverlay(compositor_, it->second.art, false);
    if (std::holds_alternative<TextInput>(it->second.placed.item)) return;
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
