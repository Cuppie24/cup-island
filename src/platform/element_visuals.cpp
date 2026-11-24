#include "platform/element_visuals.h"

#include "core/liquid_glass.h"

#include "core/motion.h"
#include "core/tokens.h"
#include "platform/element_painting.h"
#include "platform/field_painting.h"
#include "platform/element_parts.h"
#include "platform/text_format.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <unordered_set>

namespace cup::platform {
namespace {

// Pixel snapping (decision 024): sizes up to whole pixels, positions to the nearest one.
Vec2 pixelSize(Vec2 size) { return { std::ceil(size.x), std::ceil(size.y) }; }
// Snapped as sizeOf's edges are (snapEdge, decision 105): rounded straight
// here while the size was snapped there, an edge at 243.49998 put the
// visual at 243 and sized it from 244, a pixel short: a dark gap under a
// group's row, a light overlap above it.
wfn::float3 pixelOffset(float x, float y) { return { snapEdge(x), snapEdge(y), 0.0f }; }

// Its size in whole pixels, from both of its edges rounded, as its offset is
// (pixelOffset): rounding the size up instead grew it at the bottom and right,
// so a button's hover and glyph sat up to a pixel low in their row (decision 071).
// Snapped where the visual really is, in its parent: in a list, the scroller.
// The scrolled frame moves by fractions as a list scrolls, so its snapped
// height flipped by a pixel and rebuilt (faded in) its rows: they flickered.
Vec2 sizeOf(const Placed& p)
{
    Vec2 at = ListViewports::positionOf(p);
    Rect snapped = snapToPixels({ at.x, at.y, p.frame.width, p.frame.height });
    return { snapped.width, snapped.height };
}

bool isRing(const Item& item)
{
    auto* progress = std::get_if<Progress>(&item);
    return progress && progress->shape == ProgressShape::Ring;
}

bool isWavy(const Item& item) // a wavy bar (decision 167)
{
    auto* progress = std::get_if<Progress>(&item);
    return progress && progress->wavy && progress->shape == ProgressShape::Bar;
}

// Live data's colour: the content's accent, else the live colour (decision 010).
Color accentOf(const Placed& p) { return p.accent.value_or(tokens().liveColor); }

// A muted tab's shapes (a ring, a waveform): their colour's grey, as bright to the eye (decision 072).
Color greyOf(const Color& c)
{
    auto level = static_cast<std::uint8_t>(std::lround(0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b));
    return { level, level, level, c.a };
}
Color toned(const Color& c, bool grey) { return grey ? greyOf(c) : c; }
float opacityOf(bool grey) { return grey ? tokens().tabMutedOpacity : 1.0f; }

// A plain button's glyph or label colour: greyed when disabled, off or idle
// (decisions 064, 094), red when destructive.
Color buttonColorOf(const Button& button, bool lit)
{
    const Tokens& t = tokens();
    bool toggledOn = button.on && *button.on, toggledOff = button.on && !*button.on;
    bool idle = button.secondary && !lit && !toggledOn; // grey until hovered (decision 064) or on (decision 094)
    return !button.enabled                                    ? t.buttonDisabledColor
           : (toggledOff && !button.secondary) || idle        ? t.buttonOffColor
           : button.destructive                               ? t.dangerColor
                                                              : t.buttonColor;
}

// The one colour an item's pixels are drawn in, if it's one (decision 102):
// then they're painted white and coloured by a brush that fades. Text in
// one colour, an icon, a button's glyph or label; not an emoji (its own
// colours), a round button (two), or a picture.
std::optional<Color> inkOf(const Item& item, bool lit)
{
    const Tokens& t = tokens();
    // A dropping line (decision 180) is drawn in its colour, in a sprite per line: no brush to fade.
    if (auto* text = std::get_if<Text>(&item)) return text->drops ? std::nullopt : singleInkOf(*text);
    if (auto* icon = std::get_if<Icon>(&item)) {
        if (icon->glyph == Glyph::Fire) return std::nullopt;
        return icon->disabled ? t.buttonDisabledColor : t.iconColor;
    }
    auto* button = std::get_if<Button>(&item);
    if (!button) return std::nullopt;
    // As paint() draws it: the first that applies.
    auto emoji = [](const std::string& s) { return std::any_of(s.begin(), s.end(), [](unsigned char c) { return c >= 0xF0; }); };
    if (button->primary) return t.islandColor;
    if (button->round || emoji(button->label)) return std::nullopt;
    if (button->quiet) return t.captionTextColor;
    if (!button->label.empty()) return buttonColorOf(*button, lit);
    if (button->picture || button->glyph == Glyph::Fire) return std::nullopt;
    return buttonColorOf(*button, lit);
}

// A button's filled circle (decision 101): a primary's, in the button colour
// (softer under the pointer, decision 066), or a toggle's, tabSelectedColor
// while on and clear while off (decision 094). Nothing: it has none.
std::optional<Color> buttonFillOf(const Placed& p, bool lit)
{
    auto* button = std::get_if<Button>(&p.item);
    if (!button) return std::nullopt;
    const Tokens& t = tokens();
    if (button->primary) {
        Color fill = button->enabled ? t.buttonColor : t.buttonDisabledColor;
        if (lit && button->enabled) fill.a = static_cast<std::uint8_t>(std::lround(fill.a * std::clamp(t.primaryHoverOpacity, 0.0f, 1.0f)));
        return fill;
    }
    if (button->on && !button->round) {
        Color fill = t.tabSelectedColor;
        if (!*button->on) fill.a = 0;
        return fill;
    }
    return std::nullopt;
}

// The shape under a hovered button, a text field's background, the dim over clickable art.
Color hoverColorOf(const Placed& p)
{
    if (std::holds_alternative<TextInput>(p.item)) return tokens().inputColor;
    if (std::holds_alternative<Image>(p.item)) return tokens().artHoverColor;
    return tokens().buttonHoverColor;
}

// Clicked into or picked from, not pressed: no squash on press (decisions 043, 093).
bool noSquash(const Item& item)
{
    return std::holds_alternative<TextInput>(item) || std::holds_alternative<Text>(item) || std::holds_alternative<Segments>(item);
}

// A list item's delete button: hidden until the pointer is on its row (decision 020);
// a window card's handle, until it's on its card (decision 129).
bool revealsOnHover(const Item& item)
{
    auto* button = std::get_if<Button>(&item);
    return button && (button->revealOnHover || button->revealWithSlot);
}

std::string slotOf(const std::string& key) { return key.substr(0, key.find('/')); }

// A row's secondary action (revealed on hover) is quieter than its checkbox;
// the button around it, and its hover shape, stay the row's size.
float glyphSizeOf(const Button& button)
{
    const Tokens& t = tokens();
    if (button.revealOnHover) return t.revealGlyphSize;
    // Windows' caption glyphs, at the size Windows draws its own (decision 123).
    bool caption = button.glyph == Glyph::Minimize || button.glyph == Glyph::Maximize || button.glyph == Glyph::Restore || button.glyph == Glyph::CloseWindow;
    if (caption) return t.windowChromeGlyphSize;
    if (button.large) return t.largeButtonGlyphSize; // a card's main controls (decision 107)
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

// A Hero time (decision 107): its characters roll as they change.
bool rolls(const Item& item)
{
    auto* text = std::get_if<Text>(&item);
    return text && text->style == TextStyle::Hero && !text->marquee;
}

// The line being sung (decision 180): a new line drops in as a whole.
bool drops(const Item& item)
{
    auto* text = std::get_if<Text>(&item);
    return text && text->drops && !text->marquee && !rolls(item);
}

} // namespace

void ElementVisuals::init(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent)
{
    compositor_ = compositor;
    device_ = &device;
    parent_ = parent;
    lists_.init(compositor, parent);
}

void ElementVisuals::apply(const std::vector<Placed>& elements, Ms now, Ms reveal, bool glide, Ms glideFor)
{
    reveal_ = reveal;
    glide_ = glide;
    glideFor_ = glideFor;
    deviceLost_ = false;
    std::uint64_t generation = device_->generation(); // a reset found in createSurface shows only here (decision 164)

    // Laid out again on every frame of a window's resize: a set, not a search per visual (decision 117).
    std::unordered_set<std::string_view> shown;
    shown.reserve(elements.size());
    for (const Placed& p : elements) shown.insert(p.key);
    for (auto it = views_.begin(); it != views_.end();) {
        if (shown.contains(it->first)) { ++it; continue; }
        fadeOutAndRemove(it->second.visual);
        it = views_.erase(it);
    }

    // What's new arrives top row first (decision 080).
    std::vector<float> tops;
    for (const Placed& placed : elements)
        if (!views_.contains(placed.key)) tops.push_back(placed.frame.y);
    std::vector<Ms> delays = staggerDelays(tops, tokens());
    std::size_t arriving = 0;

    for (const Placed& placed : elements) {
        auto it = views_.find(placed.key);
        bool fresh = it == views_.end();
        if (fresh) views_.emplace(placed.key, create(placed, now, true, arriving < delays.size() ? std::optional<Ms>(delays[arriving++]) : std::nullopt));
        else fresh = update(it->second, placed, now);
        if (fresh && placed.key == hovered_) showHover(hovered_, true); // rebuilt under the pointer
    }

    lists_.prune(elements);

    // The GPU was reset mid-apply: the device is rebuilt, but every surface lost its pixels.
    if (deviceLost_ || device_->generation() != generation)
        for (auto& [key, view] : views_)
            if (view.surface || view.rolling.visual || view.dropping.visual) paint(view);
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

ElementVisuals::View ElementVisuals::create(const Placed& placed, Ms now, bool fade, std::optional<Ms> arrive)
{
    const Tokens& t = tokens();
    View view;
    view.placed = placed;
    view.grey = greyNow(placed);
    Color accent = toned(accentOf(placed), view.grey);
    // Whole pixels: frames are fractional (islandScale x display scale), and a
    // surface stretched into a fractional box is resampled, which blurs text.
    Vec2 size = pixelSize(sizeOf(placed)), visualSize = size;

    if (auto* toggle = std::get_if<Switch>(&placed.item)) {
        view.toggle = createSwitch(compositor_, size, toggle->on);
        view.visual = view.toggle.visual;
    } else if (auto* segments = std::get_if<Segments>(&placed.item)) {
        // Its pill a shape that slides, its labels twice (decision 093).
        view.surface = device_->createSurface(size);
        view.segments = createSegments(compositor_, view.surface, device_->createSurface(size), size, *segments);
        view.visual = view.segments.visual;
    } else if (std::holds_alternative<Dial>(placed.item)) {
        // A Dial (decision 065): the ring's shapes, with its time painted on a surface laid over them.
        RingParts parts = createRing(compositor_, size, accent);
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
        RingParts parts = createRing(compositor_, size, accent);
        view.visual = parts.visual;
        view.arc = parts.arc;
        view.track = parts.track;
        view.accent = parts.accent;
    } else if (std::holds_alternative<Progress>(placed.item)) {
        ProgressParts parts = createProgress(compositor_, size, accent, isWavy(placed.item), std::get<Progress>(placed.item).seekable,
                                             device_->d2dFactory());
        view.wave = parts.wave;
        view.pinned = parts.pinned;
        view.lead = progressOverhang();
        visualSize.x += 2 * view.lead;
        view.visual = parts.visual;
        view.fill = parts.fill;
        view.knob = parts.knob;
        view.track = parts.track;
        view.accent = parts.accent;
        view.knobBrush = parts.knobBrush;
    } else if (std::holds_alternative<Waveform>(placed.item)) {
        WaveformParts parts = createWaveform(compositor_, size, accent);
        view.visual = parts.visual;
        view.waveform = parts.visual;
        view.bars = parts.bars;
        view.accent = parts.accent;
    } else if (std::holds_alternative<Button>(placed.item) || std::holds_alternative<TextInput>(placed.item)) {
        // A text field is built like a button: its background is the hover shape, shown only under the pointer.
        view.surface = device_->createSurface(size);
        auto* button = std::get_if<Button>(&placed.item); // a primary one shows hover in its own fill (decision 066)
        ButtonParts parts = createButton(compositor_, size, placed.cornerRadius, view.surface, hoverColorOf(placed));
        view.visual = parts.visual;
        view.hover = parts.hover;
        // Lit by its row, or by its fill: stays hidden. A text edited in place has none (decision 122): its selection shows it.
        auto* input = std::get_if<TextInput>(&placed.item);
        view.highlight = placed.litByRow || placed.noHoverShape || (button && button->primary) || (input && input->inPlace) ? nullptr : parts.highlight;
        view.glyphHost = parts.glyph;
        if (shiny_ && input && solidBacked(*input)) {
            // On the island, a search card's field: the island's colour, solid, under its see-through box, in the
            // same shape and fading with it, so the card's glow doesn't show through the field (decision 176).
            auto box = parts.highlight.Shapes().GetAt(0).as<wuc::CompositionSpriteShape>();
            view.backing = compositor_.CreateColorBrush(toUiColor(tokens().islandColor));
            auto under = compositor_.CreateSpriteShape(box.Geometry());
            under.FillBrush(view.backing);
            parts.highlight.Shapes().InsertAt(0, under);
        }
        syncFill(view, Ms{ 0 }); // a primary's or a toggle's circle, under the glyph
        if (auto ink = inkOf(placed.item, false)) { // its glyph or label in one colour: painted white, coloured through (decision 102)
            view.ink = compositor_.CreateColorBrush(toUiColor(*ink));
            view.inkTarget = *ink;
            auto masked = compositor_.CreateMaskBrush();
            masked.Source(view.ink);
            masked.Mask(parts.glyph.Brush());
            parts.glyph.Brush(masked);
        }
    } else if (rolls(placed.item)) {
        // A sprite per character, so each can roll (decision 107); it reaches
        // `lead` past the frame on each side, room for a heavy glyph's overhang.
        view.rolling = createRollingText(compositor_, *device_, std::get<Text>(placed.item), size, view.grey);
        view.lead = view.rolling.lead;
        visualSize.x += 2 * view.lead;
        view.visual = view.rolling.visual;
    } else if (drops(placed.item)) {
        // A sprite per line, so the old one can drop out as the new one falls in (decision 180).
        view.dropping = createDroppingText(compositor_, *device_, std::get<Text>(placed.item), size, view.grey);
        view.visual = view.dropping.visual;
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
            std::optional<Color> ink = inkOf(placed.item, false);
            auto inkBrush = ink ? compositor_.CreateColorBrush(toUiColor(*ink)) : wuc::CompositionColorBrush{ nullptr };
            MarqueeBrush made = createMarqueeBrush(compositor_, view.surface, visualSize, view.lead, width + t.marqueeGap, inkBrush);
            sprite.Brush(made.brush);
            if (made.inked) { // its colour fades too (decision 102)
                view.ink = inkBrush;
                view.inkTarget = *ink;
            }
        } else {
            view.surface = device_->createSurface(size);
            auto pixels = compositor_.CreateSurfaceBrush(view.surface);
            if (auto ink = inkOf(placed.item, false)) {
                // One colour (a text, an icon): painted white, coloured by a brush through it, which fades (decision 102).
                view.ink = compositor_.CreateColorBrush(toUiColor(*ink));
                view.inkTarget = *ink;
                auto masked = compositor_.CreateMaskBrush();
                masked.Source(view.ink);
                masked.Mask(pixels);
                sprite.Brush(masked);
            } else {
                sprite.Brush(pixels);
            }
        }
        view.visual = sprite;
        if (std::holds_alternative<Image>(placed.item)) view.picture = sprite;
    }
    if (placed.raised && view.picture) { // the expanded card's cover: a soft shadow under it (decision 094)
        // Clickable art is a container already. Plain art is a sprite, whose children
        // would draw over its picture: it goes into a container, over the shadow.
        wuc::ContainerVisual holder = view.art.visual;
        if (!holder) {
            holder = compositor_.CreateContainerVisual();
            view.picture.Size(toFloat2(size)); // already whole pixels
            holder.Children().InsertAtTop(view.picture);
            view.visual = holder;
        }
        view.shadowShape = device_->createSurface(size); // painted with the rest, in paint()
        view.shadow = addArtShadow(compositor_, holder, size, view.shadowShape);
    }
    place(view);
    view.visual.Size(toFloat2(pixelSize(visualSize)));
    view.visual.CenterPoint({ visualSize.x / 2, visualSize.y / 2, 0 });
    // A group's background goes under everything, even when it's rebuilt after its row's content (decision 090).
    // One carried in a window (decision 134) goes over everything, its card's background too.
    if (std::holds_alternative<GroupFill>(placed.item) && !placed.carried) lists_.parentFor(placed).Children().InsertAtBottom(view.visual);
    else lists_.parentFor(placed).Children().InsertAtTop(view.visual);
    if (placed.list) lists_.setGliding(view.visual, !placed.list->dragged); // after its first place: it appears there

    if (view.grey && view.track) view.track.Color(toUiColor(greyOf(t.trackColor)));
    if (view.grey && view.knobBrush) view.knobBrush.Color(toUiColor(greyOf(t.knobColor)));
    if (view.surface) paint(view);
    if (view.fill) armProgress(compositor_, view.fill, std::get<Progress>(placed.item).timeline, size.x, now);
    if (view.wave.geometry) { // as it is, with no rising
        const Timeline& timeline = std::get<Progress>(placed.item).timeline;
        ripple(compositor_, view.wave, timeline.playing, timeline.fractionAt(now) * size.x, true);
    }
    if (view.arc) armRing(compositor_, view.arc, ringTimeline(placed.item), now);
    if (!view.bars.empty()) animateBars(view);
    if (view.highlight && focusedField(placed.item)) view.highlight.Opacity(1.0f);
    if (revealsOnHover(placed.item) && !revealedNow(placed)) view.visual.Opacity(0.0f); // until its row (or card) is hovered
    else if (fade) fadeIn(compositor_, view.visual, opacityOf(view.grey), reveal_ + arrive.value_or(Ms{ 0 }), arrive.has_value());
    else view.visual.Opacity(opacityOf(view.grey));
    return view;
}

bool ElementVisuals::update(View& view, const Placed& placed, Ms now)
{
    // Its real size, not its snapped one: a move by a fraction is no reason to rebuild it.
    bool resized = view.placed.frame.width != placed.frame.width || view.placed.frame.height != placed.frame.height ||
                   view.placed.cornerRadius != placed.cornerRadius;
    // A bar and a ring are both Progress, but different visuals. A new accent
    // (a cover's colour) isn't a new visual: its brushes fade to it.
    bool retyped = view.placed.item.index() != placed.item.index() || isRing(view.placed.item) != isRing(placed.item) ||
                   isWavy(view.placed.item) != isWavy(placed.item) || // another bar: a wave, another knob
                   (isWavy(placed.item) && std::get<Progress>(view.placed.item).seekable != std::get<Progress>(placed.item).seekable) ||
                   clickableArt(view.placed.item) != clickableArt(placed.item) || rolls(view.placed.item) != rolls(placed.item) ||
                   drops(view.placed.item) != drops(placed.item);
    bool recoloured = view.placed.accent != placed.accent;
    // In a list, its place is in the list's scroller: scrolling moves the scroller, not it.
    Vec2 was = lists_.positionOf(view.placed), at = lists_.positionOf(placed);
    bool moved = was.x != at.x || was.y != at.y;
    // Moved by a fraction, its whole-pixel size (taken from where it is, both
    // edges rounded) can come out a pixel different. Kept at the old size,
    // neighbours that should meet overlapped or left a gap: a line across a
    // group's background where two rows' pieces join (decision 090).
    bool resnapped = moved && sizeOf(view.placed) != sizeOf(placed);
    bool changed = !(view.placed.item == placed.item);
    auto pictureOf = [](const Item& item) { auto* image = std::get_if<Image>(&item); return image ? image->data : nullptr; };
    bool newPicture = view.picture && pictureOf(view.placed.item) != pictureOf(placed.item);
    // A text that gains a second colour (bright digits, a detail) can't be one ink any more, or the reverse (decision 102).
    bool reinked = inkOf(view.placed.item, false).has_value() != inkOf(placed.item, false).has_value();
    // Lit by its row or not (a field focused, decision 122): its own box comes or goes, so it's built anew.
    bool relit = view.placed.litByRow != placed.litByRow;
    bool relisted = view.placed.list.has_value() != placed.list.has_value() ||
                    (placed.list && (view.placed.list->id != placed.list->id || view.placed.activityId != placed.activityId));
    if (resized || retyped || relisted || reinked || resnapped || relit || (changed && (swims(view.placed.item) || swims(placed.item)))) {
        // A different kind of visual, a new size (tokens.json edits), another list or a new swimming string: rebuild it.
        if (auto parent = view.visual.Parent()) parent.Children().Remove(view.visual);
        // A to-do's text turning into its field (or back) swaps in place: a fade would flash it (decision 043).
        // Only its pixels' size changed (resnapped): the same thing, in place, with no fade either.
        auto editable = [](const Item& item) { return std::holds_alternative<Text>(item) || std::holds_alternative<TextInput>(item); };
        // Only resized (a window being resized, decision 120): the same thing at its new size, in place;
        // faded in anew on every frame of the drag, it flickered.
        bool inPlace = (retyped && editable(view.placed.item) && editable(placed.item)) ||
                       ((resnapped || resized) && !retyped && !relisted && !reinked && !changed) ||
                       (relit && !retyped && !relisted); // a field clicked into: its box, in place
        // A card's cover given another size (decision 180): the new one grows or shrinks from the old one's size and place.
        bool regrown = glide_ && glideFor_ > Ms{ 0 } && resized && !retyped && !relisted && std::holds_alternative<Image>(placed.item) && !placed.list;
        Vec2 oldSize = pixelSize(sizeOf(view.placed));
        view = create(placed, now, !inPlace && !regrown);
        if (regrown)
            resizeFrom(compositor_, view.visual, oldSize, pixelSize(sizeOf(placed)), pixelOffset(was.x - view.lead, was.y),
                       pixelOffset(at.x - view.lead, at.y), glideFor_);
        // Rebuilt somewhere else on a gliding page (a card grown as a task is dragged in, decision 134): eased there too.
        else if (moved && glide_ && !placed.carried && !placed.list)
            glideFrom(compositor_, view.visual, pixelOffset(was.x - view.lead, was.y), pixelOffset(at.x - view.lead, at.y), glideFor_);
        return true;
    }
    bool wasFocused = focusedField(view.placed.item);
    Timeline wasRing = ringTimeline(view.placed.item);
    bool wasRevealed = revealsOnHover(view.placed.item);
    bool wasDragged = view.placed.list && view.placed.list->dragged, dragged = placed.list && placed.list->dragged;
    auto parent = lists_.parentFor(placed); // keeps the list's window and scroll in step
    // Picked up in a window (decision 134): over everything, as a list's dragged row is.
    if (placed.carried && !view.placed.carried) {
        parent.Children().Remove(view.visual);
        parent.Children().InsertAtTop(view.visual);
    }
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
        view.visual.Opacity(revealsOnHover(placed.item) && !revealedNow(placed) ? 0.0f : 1.0f);
    }
    if (newPicture) { // a cover read after its track showed (decision 056): painted anew, fading in over the old
        auto size = view.surface.Size();
        view.surface = device_->createSurface({ size.Width, size.Height });
        paint(view);
        fadeInSurface(view.picture, view.surface, tokens().mediaArtFade);
    }
    // A list's rows glide in their scroller already; a carried one follows the pointer (decision 132).
    if (moved && glide_ && !placed.carried && !placed.list)
        glideFrom(compositor_, view.visual, pixelOffset(was.x - view.lead, was.y), pixelOffset(at.x - view.lead, at.y), glideFor_);
    else if (moved)
        place(view);
    if (greyNow(placed) != view.grey) setGrey(view, !view.grey); // its tab became the one shown, or stopped being it
    else if (recoloured) recolor(view); // redraws its pixels too (a new picture: already in its fresh surface)
    if (!changed) return false;
    if (view.toggle.knob) slideSwitch(compositor_, view.toggle, pixelSize(sizeOf(placed)), std::get<Switch>(placed.item).on);
    if (view.segments.pill) slideSegments(compositor_, view.segments, pixelSize(sizeOf(placed)), std::get<Segments>(placed.item));
    if (view.highlight && focusedField(placed.item) != wasFocused) // focus came or went: background follows (or hover keeps it)
        fadeTo(compositor_, view.highlight, focusedField(placed.item) || placed.key == hovered_ ? 1.0f : 0.0f, tokens().hoverFadeDuration);
    if (view.surface && !newPicture) paint(view);
    if (view.rolling.visual && !rollTo(compositor_, *device_, view.rolling, std::get<Text>(placed.item), pixelSize(sizeOf(placed))))
        deviceLost_ = true;
    if (view.dropping.visual && !dropTo(compositor_, *device_, view.dropping, std::get<Text>(placed.item), pixelSize(sizeOf(placed))))
        deviceLost_ = true;
    syncFill(view, tokens().hoverFadeDuration); // a toggle switched on or off, a button enabled
    if (view.ink) fadeInk(view, tokens().hoverFadeDuration); // another style: another colour
    // While scrubbing, the bar belongs to the pointer; endScrub() re-arms it.
    if (view.fill && !view.scrubbing) armProgress(compositor_, view.fill, std::get<Progress>(placed.item).timeline, placed.frame.width, now);
    // Paused or playing again: the wave lies flat or rises (decision 167). No change: left running, not restarted.
    if (view.wave.geometry) {
        const Timeline& timeline = std::get<Progress>(placed.item).timeline;
        ripple(compositor_, view.wave, timeline.playing, timeline.fractionAt(now) * placed.frame.width, false);
    }
    // A Dial changes every second (its time); its ring is re-armed only when its timeline does.
    if (view.arc && (!std::holds_alternative<Dial>(placed.item) || ringTimeline(placed.item) != wasRing))
        armRing(compositor_, view.arc, ringTimeline(placed.item), now);
    if (!view.bars.empty()) animateBars(view);
    return false;
}

void ElementVisuals::recolor(View& view)
{
    const Tokens& t = tokens();
    if (view.track) fadeColor(view.track, toned(t.trackColor, view.grey));
    if (view.accent) fadeColor(view.accent, toned(accentOf(view.placed), view.grey));
    if (view.knobBrush) fadeColor(view.knobBrush, toned(t.knobColor, view.grey));
    if (view.hover) fadeColor(view.hover, hoverColorOf(view.placed));
    if (view.backing) fadeColor(view.backing, t.islandColor);
    if (view.shadow) fadeArtShadow(compositor_, view.shadow);
    if (view.toggle.knob) {
        bool on = std::get<Switch>(view.placed.item).on;
        fadeColor(view.toggle.track, switchTrackColor(on));
        fadeColor(view.toggle.knobBrush, switchKnobColor(on));
    }
    if (view.segments.pill) recolorSegments(view.segments, std::get<Segments>(view.placed.item));
    // A button's filled circle is a shape: it fades with the rest (decision
    // 101). Text and glyphs are pixels in a surface: redrawn in their new
    // colours at once (a crossfade of two copies looked like one laid over the other).
    syncFill(view, recolorDuration(t));
    if (view.ink) { // one colour (text, an icon, a glyph): its brush fades, its white pixels stay (decision 102)
        fadeInk(view, recolorDuration(t));
        return;
    }
    if (view.surface || view.rolling.visual || view.dropping.visual) paint(view);
}

void ElementVisuals::fadeInk(View& view, Ms duration)
{
    std::optional<Color> ink = inkOf(view.placed.item, view.lit);
    if (!ink || *ink == view.inkTarget) return; // unchanged: a fade under way carries on
    view.inkTarget = *ink;
    fadeColor(view.ink, *ink, duration);
}

void ElementVisuals::syncFill(View& view, Ms duration)
{
    std::optional<Color> fill = buttonFillOf(view.placed, view.lit);
    auto* button = std::get_if<Button>(&view.placed.item);
    // On the island, liquid glass: a filled control's edge shines, and so
    // does a large plain one's frame (the skips; decision 165).
    bool ring = shiny_ && button && liquidRing(*button);
    float shine = fill || ring ? liquidShine(tokens()) : 0.0f;
    if (!view.buttonFill) {
        if ((!fill && !ring) || !view.glyphHost) return;
        if (view.shineHost && !fill) { // a ring already, still no fill
            fadeShine(view.shine, shine, duration);
            return;
        }
        // Made once, the first time it has a colour: a circle in the glyph's
        // whole pixels (where the glyph is centred), under it.
        Vec2 size = pixelSize(sizeOf(view.placed));
        float r = std::min(size.x, size.y) / 2;
        auto visual = compositor_.CreateShapeVisual();
        visual.Size(toFloat2(size));
        if (fill) {
            auto circle = compositor_.CreateEllipseGeometry();
            circle.Center({ size.x / 2, size.y / 2 });
            circle.Radius({ r, r });
            view.buttonFill = compositor_.CreateColorBrush(toUiColor(*fill)); // new: set, not faded
            auto shape = compositor_.CreateSpriteShape(circle);
            shape.FillBrush(view.buttonFill);
            visual.Shapes().Append(shape);
        }
        if (shiny_ && !view.shineHost) {
            view.shine = makeShine(compositor_);
            // Round the fill's circle; round a plain button's own frame.
            if (fill) placeShine(view.shine, { size.x / 2 - r, size.y / 2 - r, 2 * r, 2 * r }, r);
            else placeShine(view.shine, { 0, 0, size.x, size.y }, view.placed.cornerRadius);
            visual.Shapes().Append(view.shine.shape);
            view.shineHost = true;
            fadeShine(view.shine, shine, Ms{ 1 }); // new: there at once
        }
        view.glyphHost.Parent().Children().InsertBelow(visual, view.glyphHost);
        return;
    }
    fadeColor(view.buttonFill, fill.value_or(Color{ 0, 0, 0, 0 }), duration); // none now: clear
    fadeShine(view.shine, shine, duration);                                  // with its circle
}

void ElementVisuals::paint(View& view)
{
    const Tokens& t = tokens();
    Vec2 size = sizeOf(view.placed);
    IDWriteFactory* dw = device_->dwrite();
    const Item& item = view.placed.item;
    if (view.rolling.visual) { // its characters' own surfaces (decision 107)
        view.rolling.grey = view.grey;
        if (!repaintRolling(*device_, view.rolling, pixelSize(size))) deviceLost_ = true;
        return;
    }
    if (view.dropping.visual) { // the line being sung: its own sprite's surface (decision 180)
        view.dropping.grey = view.grey;
        if (!repaintDropping(*device_, view.dropping, pixelSize(size))) deviceLost_ = true;
        return;
    }
    // Coloured by its brush (decision 102): its pixels are white.
    std::optional<Color> white = view.ink ? std::optional<Color>(Color{ 255, 255, 255 }) : std::nullopt;
    auto content = [&](ID2D1DeviceContext* dc) {
        if (auto* text = std::get_if<Text>(&item)) {
            if (view.marqueeWidth > 0) paintMarquee(dc, dw, *text, view.marqueeWidth, view.lead, t.marqueeGap, size.y, white);
            else paintText(dc, dw, *text, size, white);
        } else if (auto* icon = std::get_if<Icon>(&item)) {
            Color color = icon->disabled ? t.buttonDisabledColor : t.iconColor;
            paintGlyph(dc, dw, icon->glyph, icon->large ? t.largeIconSize : t.iconSize, pixelSize(size), white.value_or(color)); // the surface's whole pixels
        } else if (auto* button = std::get_if<Button>(&item)) {
            Color color = white.value_or(buttonColorOf(*button, view.lit));
            // A primary's and a toggle's circle is a shape under this (syncFill, decision 101).
            if (button->primary) paintPrimaryButton(dc, dw, *button, pixelSize(size), white);
            else if (button->round) paintRoundButton(dc, dw, *button, size);
            else if (button->quiet) paintLabel(dc, dw, button->label, size, white.value_or(t.captionTextColor), TextStyle::Caption); // a way into more (decision 063)
            else if (!button->label.empty()) paintLabel(dc, dw, button->label, size, color);
            else if (button->picture) paintPicture(dc, button->picture.get(), size, t.resultIconSize); // a result's own icon (decision 079)
            // Centred in the surface's whole pixels, like the hover shape around it, not the fractional frame.
            else paintGlyph(dc, dw, button->glyph, glyphSizeOf(*button), pixelSize(size), color);
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
            paintDivider(dc, size);
        } else if (auto* segments = std::get_if<Segments>(&item)) {
            paintSegmentLabels(dc, dw, *segments, size, segments->enabled ? t.bodyTextColor : t.buttonDisabledColor);
        } else if (auto* keys = std::get_if<Keys>(&item)) {
            paintKeys(dc, dw, *keys, size);
        } else if (auto* fill = std::get_if<GroupFill>(&item)) {
            paintGroupFill(dc, *fill, pixelSize(size), view.placed.cornerRadius);
        } else if (auto* swatch = std::get_if<Swatch>(&item)) {
            paintSwatch(dc, *swatch, size);
        }
    };
    // A timer's big digits are drawn as smooth outlines (decision 107): a Dial's, a Display or Hero text.
    auto* text = std::get_if<Text>(&item);
    bool bigText = std::holds_alternative<Dial>(item) || (text && (text->style == TextStyle::Display || text->style == TextStyle::Hero));
    bool drawn = device_->draw(view.surface, [&](ID2D1DeviceContext* dc) {
        if (view.grey) paintGrey(dc, content); // a muted tab's (decision 072)
        else content(dc);
    }, bigText);
    if (auto* segments = std::get_if<Segments>(&item); drawn && segments && view.segments.picked) // the labels inside the pill
        drawn = device_->draw(view.segments.picked, [&](ID2D1DeviceContext* dc) { paintSegmentLabels(dc, dw, *segments, size, t.islandColor); });
    if (drawn && view.shadowShape) // the shadow's mask: opaque where the art is, its corners too
        drawn = device_->draw(view.shadowShape, [&](ID2D1DeviceContext* dc) {
            float r = view.placed.cornerRadius;
            Vec2 whole = pixelSize(size); // the surface's size
            dc->FillRoundedRectangle(D2D1_ROUNDED_RECT{ D2D1::RectF(0, 0, whole.x, whole.y), r, r }, brushOf(dc, Color{ 0, 0, 0, 255 }).get());
        });
    if (drawn && view.glyphSurface)
        drawn = device_->draw(view.glyphSurface, [&](ID2D1DeviceContext* dc) {
            paintGlyph(dc, dw, Glyph::OpenApp, t.artHoverGlyphSize, pixelSize(size), t.artHoverGlyphColor);
        });
    if (!drawn) deviceLost_ = true;
    if (std::holds_alternative<TextInput>(item)) syncCaret(view);
}

// The caret (decision 131): a sprite of its own on the field, so it blinks on the compositor and
// an idle field still costs nothing between keys (decision 016). Shown steady at each change (a key,
// a click), then blinking at Windows' own rate; Windows set to not blink, it doesn't.
void ElementVisuals::syncCaret(View& view)
{
    auto* input = std::get_if<TextInput>(&view.placed.item);
    auto container = view.visual.try_as<wuc::ContainerVisual>();
    if (!input || !container) return;
    std::optional<Rect> at = caretRect(device_->dwrite(), *input, sizeOf(view.placed));
    if (!at) {
        if (view.caret) {
            container.Children().Remove(view.caret);
            view.caret = nullptr;
        }
        return;
    }
    if (!view.caret) {
        view.caret = compositor_.CreateSpriteVisual();
        container.Children().InsertAtTop(view.caret);
    }
    view.caret.Brush(compositor_.CreateColorBrush(toUiColor(tokens().bodyTextColor)));
    view.caret.Size({ at->width, at->height });
    view.caret.Offset({ at->x, at->y, 0 });
    view.caretFrame = *at;
    // Restarted at every repaint (a key, a click moved it): visible first, as Windows' own caret.
    view.caret.StopAnimation(L"Opacity");
    view.caret.Opacity(1.0f);
    UINT blink = GetCaretBlinkTime();
    if (blink == 0 || blink == INFINITE) return; // set not to blink
    auto step = compositor_.CreateStepEasingFunction();
    auto on = compositor_.CreateScalarKeyFrameAnimation();
    on.InsertKeyFrame(0.0f, 1.0f);
    on.InsertKeyFrame(0.5f, 0.0f, step); // on for half the period, off for the other half
    on.InsertKeyFrame(1.0f, 1.0f, step);
    on.Duration(std::chrono::milliseconds(2 * blink));
    on.IterationBehavior(wuc::AnimationIterationBehavior::Forever);
    view.caret.StartAnimation(L"Opacity", on);
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

void ElementVisuals::setHoveredRow(const std::string& row, const std::string& slot)
{
    if (row == hoveredRow_ && slot == hoveredSlot_) return;
    hoveredRow_ = row;
    hoveredSlot_ = slot;
    for (auto& [key, view] : views_)
        if (revealsOnHover(view.placed.item)) fadeTo(compositor_, view.visual, revealedNow(view.placed) ? 1.0f : 0.0f, tokens().hoverFadeDuration);
}

bool ElementVisuals::revealedNow(const Placed& placed) const
{
    auto* button = std::get_if<Button>(&placed.item);
    if (button && button->revealWithSlot) return !hoveredSlot_.empty() && slotOf(placed.key) == hoveredSlot_;
    return rowOf(placed.key) == hoveredRow_;
}

bool ElementVisuals::greyNow(const Placed& placed) const
{
    return placed.muted && hoveredTab_ != placed.activityId;
}

void ElementVisuals::setHoveredTab(const std::optional<std::string>& tab)
{
    if (tab == hoveredTab_) return;
    hoveredTab_ = tab;
    for (auto& [key, view] : views_)
        if (greyNow(view.placed) != view.grey) setGrey(view, !view.grey);
}

// Grey and dim, or back to colour (decision 072), over the hover fade: its
// opacity and shapes fade; a picture crossfades to its new pixels, text is redrawn.
void ElementVisuals::setGrey(View& view, bool grey)
{
    const Tokens& t = tokens();
    view.grey = grey;
    fadeTo(compositor_, view.visual, opacityOf(grey), t.hoverFadeDuration);
    if (view.track) fadeColor(view.track, toned(t.trackColor, grey), t.hoverFadeDuration);
    if (view.accent) fadeColor(view.accent, toned(accentOf(view.placed), grey), t.hoverFadeDuration);
    if (view.knobBrush) fadeColor(view.knobBrush, toned(t.knobColor, grey), t.hoverFadeDuration);
    if (view.rolling.visual || view.dropping.visual) paint(view); // greyed or coloured again, in place (decision 107)
    if (!view.surface) return;
    if (view.picture && view.marqueeWidth == 0) {
        auto size = view.surface.Size();
        view.surface = device_->createSurface({ size.Width, size.Height });
        paint(view);
        fadeInSurface(view.picture, view.surface, t.hoverFadeDuration);
    } else {
        paint(view);
    }
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
    if (view.knob && !view.scrubbing) showKnob(compositor_, view.knob, on, view.pinned);
    if (view.art.overlay) showArtOverlay(compositor_, view.art, on);
    if (auto* button = std::get_if<Button>(&view.placed.item); button && (button->secondary || button->primary) && view.lit != on) {
        view.lit = on; // grey to full strength and back (decision 064); a primary's fill softens (decision 066)
        paint(view);
        syncFill(view, tokens().hoverFadeDuration);
        if (view.ink) fadeInk(view, tokens().hoverFadeDuration); // grey to full strength, and back
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
    // Nor Segments: its pill's slide is the answer, and a wide control shrinking as one looked wrong (decision 093).
    if (noSquash(it->second.placed.item)) return;
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
    if (noSquash(it->second.placed.item)) return;
    playScale(compositor_, it->second.visual, 1.0f, t.pressReleaseDuration, t.pressReleaseEase);
}

// The bar never squashes on press: it's a slider, so it follows the pointer instead.
void ElementVisuals::scrub(const std::string& key, float fraction)
{
    auto it = views_.find(key);
    if (it == views_.end() || !it->second.fill) return;
    View& view = it->second;
    if (!view.scrubbing) showKnob(compositor_, view.knob, true, view.pinned);
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
    showKnob(compositor_, view.knob, key == hovered_, view.pinned);
}

} // namespace cup::platform
