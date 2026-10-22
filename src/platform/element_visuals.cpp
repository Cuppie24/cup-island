#include "platform/element_visuals.h"

#include "core/tokens.h"
#include "platform/element_painting.h"
#include "platform/field_painting.h"
#include "platform/element_parts.h"

#include <algorithm>
#include <cmath>

namespace cup::platform {
namespace {

// Pixel snapping (decision 024): sizes up to whole pixels, positions to the nearest one.
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

// The shape under a hovered button, a text field's background, the dim over clickable art.
Color hoverColorOf(const Placed& p)
{
    if (std::holds_alternative<TextInput>(p.item)) return tokens().inputColor;
    if (std::holds_alternative<Image>(p.item)) return tokens().artHoverColor;
    return tokens().buttonHoverColor;
}

// A list item's delete button: hidden until the pointer is on its row (decision 020).
bool revealsOnHover(const Item& item)
{
    auto* button = std::get_if<Button>(&item);
    return button && button->revealOnHover;
}

// A row's secondary action (revealed on hover) is quieter than its checkbox;
// the button around it, and its hover shape, stay the row's size.
float glyphSizeOf(const Button& button)
{
    const Tokens& t = tokens();
    if (button.revealOnHover) return t.revealGlyphSize;
    return button.compact ? t.iconSize : t.buttonGlyphSize;
}

// What a ring follows: a Progress ring's timeline, or a Dial's (decision 065).
Timeline ringTimeline(const Item& item)
{
    if (auto* dial = std::get_if<Dial>(&item)) return dial->timeline;
    auto* progress = std::get_if<Progress>(&item);
    return progress ? progress->timeline : Timeline{};
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
    lists_.init(compositor, parent);
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

    lists_.prune(elements);

    // The GPU was reset mid-apply: the device is rebuilt, but every surface lost its pixels.
    if (deviceLost_)
        for (auto& [key, view] : views_)
            if (view.surface) paint(view);
}

void ElementVisuals::refresh(bool coloursOnly)
{
    if (coloursOnly) {
        for (auto& [key, view] : views_) recolor(view);
        return;
    }
    for (auto& [key, view] : views_)
        if (auto parent = view.visual.Parent()) parent.Children().Remove(view.visual);
    views_.clear();
    lists_.clear();
}

ElementVisuals::View ElementVisuals::create(const Placed& placed, Ms now, bool fade)
{
    const Tokens& t = tokens();
    View view;
    view.placed = placed;
    // Whole pixels: frames are fractional (islandScale x display scale), and a
    // surface stretched into a fractional box is resampled, which blurs text.
    Vec2 size = pixelSize(sizeOf(placed)), visualSize = size;

    if (auto* toggle = std::get_if<Switch>(&placed.item)) {
        view.toggle = createSwitch(compositor_, size, toggle->on);
        view.visual = view.toggle.visual;
    } else if (std::holds_alternative<Dial>(placed.item)) {
        // A Dial (decision 065): the ring's shapes, with its time painted on a surface laid over them.
        RingParts parts = createRing(compositor_, size, accentOf(placed));
        view.visual = parts.visual;
        view.arc = parts.arc;
        view.track = parts.track;
        view.accent = parts.accent;
        view.surface = device_->createSurface(size);
        auto middle = compositor_.CreateSpriteVisual();
        middle.Brush(compositor_.CreateSurfaceBrush(view.surface));
        middle.Size(toFloat2(size));
        parts.visual.Children().InsertAtTop(middle);
    } else if (isRing(placed.item)) {
        RingParts parts = createRing(compositor_, size, accentOf(placed));
        view.visual = parts.visual;
        view.arc = parts.arc;
        view.track = parts.track;
        view.accent = parts.accent;
    } else if (std::holds_alternative<Progress>(placed.item)) {
        ProgressParts parts = createProgress(compositor_, size, accentOf(placed));
        view.lead = progressOverhang();
        visualSize.x += 2 * view.lead;
        view.visual = parts.visual;
        view.fill = parts.fill;
        view.knob = parts.knob;
        view.track = parts.track;
        view.accent = parts.accent;
        view.knobBrush = parts.knobBrush;
    } else if (std::holds_alternative<Waveform>(placed.item)) {
        WaveformParts parts = createWaveform(compositor_, size, accentOf(placed));
        view.visual = parts.visual;
        view.waveform = parts.visual;
        view.bars = parts.bars;
        view.accent = parts.accent;
    } else if (std::holds_alternative<Button>(placed.item) || std::holds_alternative<TextInput>(placed.item)) {
        // A text field is built like a button: its background is the hover shape, shown only under the pointer.
        view.surface = device_->createSurface(size);
        ButtonParts parts = createButton(compositor_, size, placed.cornerRadius, view.surface, hoverColorOf(placed));
        view.visual = parts.visual;
        view.hover = parts.hover;
        view.highlight = placed.litByRow ? nullptr : parts.highlight; // lit by its row instead (Placed::litByRow): stays hidden
    } else if (clickableArt(placed.item)) {
        view.surface = device_->createSurface(size);
        view.glyphSurface = device_->createSurface(size);
        view.art = createActionArt(compositor_, size, placed.cornerRadius, view.surface, view.glyphSurface);
        view.visual = view.art.visual;
        view.picture = view.art.picture;
        view.hover = view.art.dim;
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
        if (std::holds_alternative<Image>(placed.item)) view.picture = sprite;
    }
    place(view);
    view.visual.Size(toFloat2(pixelSize(visualSize)));
    view.visual.CenterPoint({ visualSize.x / 2, visualSize.y / 2, 0 });
    lists_.parentFor(placed).Children().InsertAtTop(view.visual);
    if (placed.list) lists_.setGliding(view.visual, !placed.list->dragged); // after its first place: it appears there

    if (view.surface) paint(view);
    if (view.fill) armProgress(compositor_, view.fill, std::get<Progress>(placed.item).timeline, size.x, now);
    if (view.arc) armRing(compositor_, view.arc, ringTimeline(placed.item), now);
    if (!view.bars.empty()) animateBars(view);
    if (view.highlight && focusedField(placed.item)) view.highlight.Opacity(1.0f);
    if (revealsOnHover(placed.item) && rowOf(placed.key) != hoveredRow_) view.visual.Opacity(0.0f); // until its row is hovered
    else if (fade) fadeIn(compositor_, view.visual);
    return view;
}

bool ElementVisuals::update(View& view, const Placed& placed, Ms now)
{
    bool resized = sizeOf(view.placed) != sizeOf(placed) || view.placed.cornerRadius != placed.cornerRadius;
    // A bar and a ring are both Progress, but different visuals. A new accent
    // (a cover's colour) isn't a new visual: its brushes fade to it.
    bool retyped = view.placed.item.index() != placed.item.index() || isRing(view.placed.item) != isRing(placed.item) ||
                   clickableArt(view.placed.item) != clickableArt(placed.item);
    bool recoloured = view.placed.accent != placed.accent;
    // In a list, its place is in the list's scroller: scrolling moves the scroller, not it.
    Vec2 was = lists_.positionOf(view.placed), at = lists_.positionOf(placed);
    bool moved = was.x != at.x || was.y != at.y;
    bool changed = !(view.placed.item == placed.item);
    auto pictureOf = [](const Item& item) { auto* image = std::get_if<Image>(&item); return image ? image->data : nullptr; };
    bool newPicture = view.picture && pictureOf(view.placed.item) != pictureOf(placed.item);
    bool relisted = view.placed.list.has_value() != placed.list.has_value() ||
                    (placed.list && (view.placed.list->id != placed.list->id || view.placed.activityId != placed.activityId));
    if (resized || retyped || relisted || (changed && (swims(view.placed.item) || swims(placed.item)))) {
        // A different kind of visual, a new size (tokens.json edits), another list or a new swimming string: rebuild it.
        if (auto parent = view.visual.Parent()) parent.Children().Remove(view.visual);
        // A to-do's text turning into its field (or back) swaps in place: a fade would flash it (decision 043).
        auto editable = [](const Item& item) { return std::holds_alternative<Text>(item) || std::holds_alternative<TextInput>(item); };
        view = create(placed, now, !(retyped && editable(view.placed.item) && editable(placed.item)));
        return true;
    }
    bool wasFocused = focusedField(view.placed.item);
    Timeline wasRing = ringTimeline(view.placed.item);
    bool wasRevealed = revealsOnHover(view.placed.item);
    bool wasDragged = view.placed.list && view.placed.list->dragged, dragged = placed.list && placed.list->dragged;
    auto parent = lists_.parentFor(placed); // keeps the list's window and scroll in step
    if (dragged != wasDragged) {
        lists_.setGliding(view.visual, !dragged); // picked up: follows the pointer; let go: glides to its place
        if (dragged) { // carried over the others
            parent.Children().Remove(view.visual);
            parent.Children().InsertAtTop(view.visual);
        }
    }
    view.placed = placed;
    // Its key now holds an item shown on hover, or one that no longer is (a page replacing the
    // card: the plan's − and + keys become days off, decision 063): its opacity follows.
    if (revealsOnHover(placed.item) != wasRevealed) {
        view.visual.StopAnimation(L"Opacity"); // a hover fade still running would carry on past this
        view.visual.Opacity(revealsOnHover(placed.item) && rowOf(placed.key) != hoveredRow_ ? 0.0f : 1.0f);
    }
    if (newPicture) { // a cover read after its track showed (decision 056): painted anew, fading in over the old
        auto size = view.surface.Size();
        view.surface = device_->createSurface({ size.Width, size.Height });
        paint(view);
        fadeInSurface(view.picture, view.surface, tokens().mediaArtFade);
    }
    if (moved) place(view);
    if (recoloured) recolor(view); // redraws its pixels too (a new picture: already in its fresh surface)
    if (!changed) return false;
    if (view.toggle.knob) slideSwitch(compositor_, view.toggle, pixelSize(sizeOf(placed)), std::get<Switch>(placed.item).on);
    if (view.highlight && focusedField(placed.item) != wasFocused) // focus came or went: background follows (or hover keeps it)
        fadeTo(compositor_, view.highlight, focusedField(placed.item) || placed.key == hovered_ ? 1.0f : 0.0f, tokens().hoverFadeDuration);
    if (view.surface && !newPicture) paint(view);
    // While scrubbing, the bar belongs to the pointer; endScrub() re-arms it.
    if (view.fill && !view.scrubbing) armProgress(compositor_, view.fill, std::get<Progress>(placed.item).timeline, placed.frame.width, now);
    // A Dial changes every second (its time); its ring is re-armed only when its timeline does.
    if (view.arc && (!std::holds_alternative<Dial>(placed.item) || ringTimeline(placed.item) != wasRing))
        armRing(compositor_, view.arc, ringTimeline(placed.item), now);
    if (!view.bars.empty()) animateBars(view);
    return false;
}

void ElementVisuals::recolor(View& view)
{
    const Tokens& t = tokens();
    if (view.track) fadeColor(view.track, t.trackColor);
    if (view.accent) fadeColor(view.accent, accentOf(view.placed));
    if (view.knobBrush) fadeColor(view.knobBrush, t.knobColor);
    if (view.hover) fadeColor(view.hover, hoverColorOf(view.placed));
    if (view.toggle.knob) {
        bool on = std::get<Switch>(view.placed.item).on;
        fadeColor(view.toggle.track, switchTrackColor(on));
        fadeColor(view.toggle.knobBrush, switchKnobColor(on));
    }
    // Text and icons are pixels in a surface: redrawn in their new colours at once.
    if (view.surface) paint(view);
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
            paintGlyph(dc, dw, icon->glyph, t.iconSize, pixelSize(size), t.iconColor); // the surface's whole pixels
        } else if (auto* button = std::get_if<Button>(&item)) {
            bool toggledOff = button->on && !*button->on;
            bool idle = button->secondary && !view.lit; // grey until hovered (decision 064)
            Color color = !button->enabled ? t.buttonDisabledColor : toggledOff || idle ? t.buttonOffColor
                        : button->destructive         ? t.dangerColor
                                                      : t.buttonColor;
            if (button->primary) paintPrimaryButton(dc, dw, *button, pixelSize(size));
            else if (button->round) paintRoundButton(dc, dw, *button, size);
            else if (button->quiet) paintLabel(dc, dw, button->label, size, t.captionTextColor, TextStyle::Caption); // a way into more (decision 063)
            else if (!button->label.empty()) paintLabel(dc, dw, button->label, size, color);
            // Centred in the surface's whole pixels, like the hover shape around it, not the fractional frame.
            else paintGlyph(dc, dw, button->glyph, glyphSizeOf(*button), pixelSize(size), color);
            if (button->on && *button->on && !button->round) paintToggleDot(dc, pixelSize(size), t.buttonGlyphSize);
        } else if (auto* image = std::get_if<Image>(&item)) {
            paintImage(dc, image->data.get(), size, view.placed.cornerRadius);
        } else if (auto* input = std::get_if<TextInput>(&item)) {
            paintInput(dc, dw, *input, size); // its background is the hover shape, nested like a button's
        } else if (auto* steps = std::get_if<Steps>(&item)) {
            paintSteps(dc, *steps, size, accentOf(view.placed));
        } else if (auto* tag = std::get_if<Tag>(&item)) {
            paintTag(dc, dw, *tag, size);
        } else if (auto* dial = std::get_if<Dial>(&item)) {
            paintDial(dc, dw, *dial, size);
        } else if (std::holds_alternative<Divider>(item)) {
            dc->FillRectangle(D2D1::RectF(0, 0, size.x, size.y), brushOf(dc, t.trackColor).get());
        } else if (auto* swatch = std::get_if<Swatch>(&item)) {
            paintSwatch(dc, *swatch, size);
        }
    });
    if (drawn && view.glyphSurface)
        drawn = device_->draw(view.glyphSurface, [&](ID2D1DeviceContext* dc) {
            paintGlyph(dc, dw, Glyph::OpenApp, t.artHoverGlyphSize, pixelSize(size), t.artHoverGlyphColor);
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
    batch.Completed([visual](auto&&, auto&&) {
        if (auto parent = visual.Parent()) parent.Children().Remove(visual); // the content, or a list's scroller
    });
}

void ElementVisuals::place(View& view)
{
    Vec2 at = lists_.positionOf(view.placed);
    view.visual.Offset(pixelOffset(at.x - view.lead, at.y));
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
    if (auto* button = std::get_if<Button>(&view.placed.item); button && button->secondary && view.lit != on) {
        view.lit = on; // grey to full strength and back (decision 064)
        paint(view);
    }
}

void ElementVisuals::press(const std::string& key)
{
    auto it = views_.find(key);
    if (it == views_.end()) return;
    const Tokens& t = tokens();
    if (it->second.highlight) fadeTo(compositor_, it->second.highlight, 1.0f, t.hoverFadeDuration);
    if (it->second.art.overlay && key != hovered_) showArtOverlay(compositor_, it->second.art, true); // pressed without a hover (touch)
    // A field, or text a click edits (decision 043), is clicked into, not pressed: no squash.
    if (std::holds_alternative<TextInput>(it->second.placed.item) || std::holds_alternative<Text>(it->second.placed.item)) return;
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
    if (std::holds_alternative<TextInput>(it->second.placed.item) || std::holds_alternative<Text>(it->second.placed.item)) return;
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
