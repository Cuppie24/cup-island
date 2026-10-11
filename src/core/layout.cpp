#include "core/layout.h"

#include "core/split.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

Vec2 sizeFor(Mode mode)
{
    const Tokens& t = tokens();
    switch (mode) {
    case Mode::Idle: return t.idleSize;
    case Mode::Compact: return t.compactSize;
    case Mode::Split: return t.compactSize;
    case Mode::Alert: return t.alertSize;
    case Mode::Expanded: return t.expandedSize;
    case Mode::Receptive: return t.receptiveSize;
    case Mode::Hidden: return t.hiddenSize;
    }
    return t.idleSize;
}

// Whose content is on screen in this mode.
struct Source {
    const Content* content = nullptr;
    std::string providerId;
    std::string activityId;
};

Source sourceFor(const IslandState& s, Mode mode)
{
    if (mode == Mode::Alert) return { &s.alert->content, s.alert->providerId, s.alert->activityId };
    if (mode != Mode::Compact && mode != Mode::Split && mode != Mode::Expanded) return {};
    // Expanded shows the activity that was opened; resting shows the top one.
    for (const auto& a : s.activities)
        if (mode != Mode::Expanded || a.id == s.expandedId) return { &a.content, a.providerId, a.id };
    return {};
}

struct SlotFrame {
    const char* name;
    const Slot* slot;
    Rect frame;
    bool square = false; // a one-glyph square (compact ends, the bubble): content always centred in it
};

// Expanded with several activities gets the host's switcher in its top row
// (PS §3.1): "next activity", plus "show in the island" unless the shown
// activity already has the pill. How many buttons that is.
int switcherButtons(const IslandState& s, Mode mode)
{
    if (mode != Mode::Expanded || s.activities.size() < 2) return 0;
    return s.activities.front().id == s.expandedId ? 1 : 2;
}

// The padding a mode keeps on every side of its content.
float paddingFor(Mode mode)
{
    const Tokens& t = tokens();
    switch (mode) {
    case Mode::Compact:
    case Mode::Split: return t.compactInset;
    case Mode::Alert: return t.alertPadding;
    case Mode::Expanded: return t.expandedPadding;
    default: return 0;
    }
}

// Where each slot sits inside the pill (PS §5.1). Every slot keeps the mode's
// padding from the pill's edges: with the nested corner radius that keeps
// rounded content concentric with the pill's corners.
std::vector<SlotFrame> slotFrames(Mode mode, Vec2 size, const Content& c, int switcherButtons)
{
    const Tokens& t = tokens();
    float p = paddingFor(mode);
    switch (mode) {
    case Mode::Compact:
    case Mode::Split: {
        float side = size.y - 2 * p, centerX = p + side + t.itemSpacing;
        return { { "compactLeading", &c.compactLeading, { p, p, side, side }, true },
                 { "compactCenter", &c.compactCenter, { centerX, p, std::max(0.0f, size.x - 2 * centerX), side } },
                 { "compactTrailing", &c.compactTrailing, { size.x - p - side, p, side, side }, true } };
    }
    case Mode::Alert: {
        float side = size.y - 2 * p, centerX = p + side + t.slotGap;
        return { { "alertLeading", &c.alertLeading, { p, p, side, side } },
                 { "alertCenter", &c.alertCenter, { centerX, p, size.x - centerX - p, side } } };
    }
    case Mode::Expanded: {
        float art = t.expandedArtSize, centerX = p + art + t.slotGap, bottomY = p + art + t.slotGap;
        float corner = t.buttonSize, cornerX = size.x - p - corner;
        float right = c.expandedTrailing.empty() ? size.x - p : cornerX - t.itemSpacing; // switcher ends here
        float centerRight = switcherButtons > 0 ? right - switcherButtons * (corner + t.itemSpacing) + t.itemSpacing - t.slotGap
                          : c.expandedTrailing.empty() ? size.x - p : cornerX - t.slotGap;
        return { { "expandedLeading", &c.expandedLeading, { p, p, art, art } },
                 { "expandedCenter", &c.expandedCenter, { centerX, p, centerRight - centerX, art } },
                 { "expandedTrailing", &c.expandedTrailing, { cornerX, p, corner, corner } },
                 { "expandedBottom", &c.expandedBottom, { p, bottomY, size.x - 2 * p, size.y - bottomY - p } } };
    }
    default:
        return {};
    }
}

float fontSize(TextStyle style)
{
    const Tokens& t = tokens();
    switch (style) {
    case TextStyle::Title: return t.titleFontSize;
    case TextStyle::Body: return t.bodyFontSize;
    case TextStyle::Caption:
    case TextStyle::Digits: return t.captionFontSize;
    }
    return t.bodyFontSize;
}

float lineHeight(TextStyle style) { return std::ceil(fontSize(style) * 1.3f); }

// Natural height of an item. Images fill the slot's square.
float heightOf(const Item& item, Rect slot)
{
    const Tokens& t = tokens();
    if (auto* text = std::get_if<Text>(&item)) return lineHeight(text->style);
    if (std::holds_alternative<Image>(item)) return std::min(slot.width, slot.height);
    if (std::holds_alternative<Button>(item)) return t.buttonSize;
    if (std::holds_alternative<Progress>(item)) return t.progressHitHeight;
    if (std::holds_alternative<TimeLabel>(item)) return lineHeight(TextStyle::Digits);
    return t.iconSize; // Icon, Waveform
}

// Fixed width, or nothing for items that stretch (Text, Progress).
std::optional<float> fixedWidthOf(const Item& item, float height)
{
    const Tokens& t = tokens();
    if (std::holds_alternative<Text>(item) || std::holds_alternative<Progress>(item)) return std::nullopt;
    if (std::holds_alternative<Image>(item)) return height;
    if (std::holds_alternative<Button>(item)) return t.buttonSize;
    if (std::holds_alternative<TimeLabel>(item)) return t.timeLabelWidth;
    return t.iconSize; // Icon, Waveform
}

Item resolve(const Item& item, Ms now)
{
    if (auto* label = std::get_if<TimeLabel>(&item)) {
        Ms position = label->timeline.positionAt(now);
        std::string value = label->remaining ? "-" + formatDuration(label->timeline.duration - position)
                                             : formatDuration(position);
        return Text{ value, TextStyle::Digits, label->remaining };
    }
    return item;
}

// Rounded content (art, button hover circles) only.
float radiusFor(const Item& item, Rect frame, float nestedRadius)
{
    if (!std::holds_alternative<Image>(item) && !std::holds_alternative<Button>(item)) return 0;
    return std::min(nestedRadius, std::min(frame.width, frame.height) / 2);
}

void placeSlot(const SlotFrame& slot, const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out)
{
    const Tokens& t = tokens();
    const Slot& rows = *slot.slot;

    std::vector<float> rowHeights;
    float total = 0;
    for (const Row& r : rows) {
        float h = 0;
        for (const auto* items : { &r.items, &r.leading, &r.trailing })
            for (const Item& item : *items) h = std::max(h, heightOf(item, slot.frame));
        rowHeights.push_back(h);
        total += h;
    }
    total += t.rowSpacing * static_cast<float>(rows.empty() ? 0 : rows.size() - 1);

    float y = slot.frame.y + (slot.frame.height - total) / 2; // centred vertically
    for (size_t r = 0; r < rows.size(); ++r) {
        const Row& row = rows[r];
        float rowHeight = rowHeights[r];
        float gap = row.loose ? t.looseItemSpacing : t.itemSpacing;

        // Keys number `items`, then `trailing`, then `leading`, so they stay stable as rows grow.
        size_t first = out.size();
        auto place = [&](const Item& item, size_t index, float x, float w) {
            float h = heightOf(item, slot.frame);
            Placed placed;
            placed.key = std::string(slot.name) + "/" + std::to_string(r) + "/" + std::to_string(index);
            placed.item = resolve(item, now);
            placed.frame = { x, y + (rowHeight - h) / 2, w, h };
            placed.cornerRadius = radiusFor(item, placed.frame, nestedRadius);
            placed.providerId = source.providerId;
            placed.activityId = source.activityId;
            if (auto* label = std::get_if<TimeLabel>(&item)) placed.ticking = label->timeline;
            out.push_back(std::move(placed));
        };
        auto widthOf = [&](const Item& item) { return fixedWidthOf(item, heightOf(item, slot.frame)); };

        // Edge groups: packed with the normal spacing, kept `gap` away from the middle.
        auto edgeWidth = [&](const std::vector<Item>& group) {
            float w = 0;
            for (const Item& item : group) w += widthOf(item).value_or(0) + t.itemSpacing;
            return group.empty() ? 0.0f : w - t.itemSpacing + gap;
        };
        float leadingWidth = edgeWidth(row.leading), trailingWidth = edgeWidth(row.trailing);
        float available = slot.frame.width - leadingWidth - trailingWidth;

        float fixed = 0;
        int stretchCount = 0;
        for (const Item& item : row.items) {
            if (auto w = widthOf(item)) fixed += *w;
            else ++stretchCount;
        }
        float spacing = gap * static_cast<float>(row.items.empty() ? 0 : row.items.size() - 1);
        float used = fixed + spacing;
        float spare = std::max(0.0f, available - used);
        float stretchWidth = stretchCount > 0 ? spare / static_cast<float>(stretchCount) : 0;

        Align align = slot.square ? Align::Center : row.align;
        float x = slot.frame.x + leadingWidth;
        if (stretchCount == 0 && align == Align::End) x += spare;
        if (stretchCount == 0 && align == Align::Center) {
            // Centred on the whole row, so edge groups don't pull the middle off
            // centre; nudged sideways only as far as needed to clear them.
            x = slot.frame.x + std::clamp((slot.frame.width - used) / 2, leadingWidth, leadingWidth + spare);
        }
        for (size_t i = 0; i < row.items.size(); ++i) {
            float w = widthOf(row.items[i]).value_or(stretchWidth);
            place(row.items[i], i, x, w);
            x += w + gap;
        }
        auto placeEdge = [&](const std::vector<Item>& group, size_t firstIndex, float x) {
            for (size_t i = 0; i < group.size(); ++i) {
                float w = widthOf(group[i]).value_or(0);
                place(group[i], firstIndex + i, x, w);
                x += w + t.itemSpacing;
            }
        };
        placeEdge(row.trailing, row.items.size(), slot.frame.x + slot.frame.width - trailingWidth + gap);
        placeEdge(row.leading, row.items.size() + row.trailing.size(), slot.frame.x);

        // Click areas meet halfway between neighbours (no dead strips) and reach
        // half a gap past the row's ends and half the row spacing above and below.
        std::vector<Placed*> placed;
        for (size_t i = first; i < out.size(); ++i) placed.push_back(&out[i]);
        std::sort(placed.begin(), placed.end(), [](const Placed* a, const Placed* b) { return a->frame.x < b->frame.x; });
        for (size_t i = 0; i < placed.size(); ++i) {
            const Rect& f = placed[i]->frame;
            float left = i == 0 ? f.x - gap / 2 : (placed[i - 1]->frame.x + placed[i - 1]->frame.width + f.x) / 2;
            float right = i + 1 == placed.size() ? f.x + f.width + gap / 2 : (f.x + f.width + placed[i + 1]->frame.x) / 2;
            placed[i]->hitFrame = { left, y - t.rowSpacing / 2, right - left, rowHeight + t.rowSpacing };
        }
        y += rowHeight + t.rowSpacing;
    }
}

// The switcher, right to left from the top-right corner (left of a
// provider's expandedTrailing button): "next", then "show in the island".
// Host-owned, so no provider id: islandEvent() maps their actions (decision 007).
void placeSwitcher(const IslandState& s, Vec2 size, const Content& c, float nestedRadius, std::vector<Placed>& out)
{
    const Tokens& t = tokens();
    float p = t.expandedPadding, side = t.buttonSize;
    float x = size.x - p - side;
    if (!c.expandedTrailing.empty()) x -= side + t.itemSpacing;
    auto place = [&](const char* key, Button button) {
        Rect frame{ x, p, side, side };
        out.push_back({ .key = key, .item = button, .frame = frame, .hitFrame = frame,
                        .cornerRadius = std::min(nestedRadius, side / 2), .activityId = s.expandedId });
        x -= side + t.itemSpacing;
    };
    place("switcher/next", { Glyph::NextActivity, kIslandNext });
    if (s.activities.front().id != s.expandedId) place("switcher/promote", { Glyph::Pin, kIslandPromote });
}

// Split: the 2nd activity's `minimal` slot, else its compact leading one
// (PS §5.1). Same shape as the pill (straight top, rounded bottom), so its
// content gets the same nested corner radius.
BubbleScene bubbleFor(const IslandState& s, Ms now)
{
    const Activity& a = s.activities[1];
    BubbleScene bubble;
    bubble.frame = bubbleHomeFrame();
    bubble.activityId = a.id;
    if (s.bubbleDrag) {
        Rect f = bubble.frame;
        bubble.dragCentre = Vec2{ f.x + f.width / 2 + s.bubbleDrag->x, f.y + f.height / 2 + s.bubbleDrag->y };
    }

    float inset = tokens().compactInset, side = bubble.frame.width - 2 * inset;
    float nestedRadius = std::max(0.0f, pillCornerRadius({ bubble.frame.width, bubble.frame.height }) - inset);
    const Slot& slot = a.content.minimal.empty() ? a.content.compactLeading : a.content.minimal;
    placeSlot({ "minimal", &slot, { inset, inset, side, side }, true }, { &a.content, a.providerId, a.id }, nestedRadius, now,
              bubble.elements);
    return bubble;
}

} // namespace

Scene layout(const IslandState& state, Ms now)
{
    Scene scene;
    scene.now = now;
    scene.mode = mode(state);
    scene.size = sizeFor(scene.mode);
    scene.offset = { (tokens().canvas.x - scene.size.x) / 2, 0 };
    // On the top edge, inside the pill's right end: the bubble shrinks into it
    // and buds back out of it hanging from the edge, like the pill, never
    // floating up from the middle of a tall card.
    float tuckInset = std::min(scene.size.x, tokens().bubbleSize) / 2;
    scene.bubbleTuck = { scene.offset.x + scene.size.x - tuckInset, scene.offset.y };
    if (scene.mode == Mode::Split) scene.bubble = bubbleFor(state, now);
    scene.dropTarget = swapPreview(state);

    Source source = sourceFor(state, scene.mode);
    if (!source.content) return scene;
    scene.label = source.content->label;
    scene.activityId = source.activityId;
    float nestedRadius = std::max(0.0f, pillCornerRadius(scene.size) - paddingFor(scene.mode));
    int buttons = switcherButtons(state, scene.mode);
    for (const SlotFrame& slot : slotFrames(scene.mode, scene.size, *source.content, buttons))
        placeSlot(slot, source, nestedRadius, now, scene.elements);
    if (buttons > 0) placeSwitcher(state, scene.size, *source.content, nestedRadius, scene.elements);
    return scene;
}

float pillCornerRadius(Vec2 size)
{
    return std::min(std::min(size.x, size.y) / 2, tokens().maxCornerRadius);
}

Rect interactiveBounds(const Scene& scene)
{
    Rect pill{ scene.offset.x, scene.offset.y, scene.size.x, scene.size.y };
    if (!scene.bubble) return pill;
    const Rect& b = scene.bubble->frame;
    float left = std::min(pill.x, b.x), top = std::min(pill.y, b.y);
    float right = std::max(pill.x + pill.width, b.x + b.width), bottom = std::max(pill.y + pill.height, b.y + b.height);
    return { left, top, right - left, bottom - top };
}

std::optional<Ms> sceneDeadline(const Scene& scene)
{
    std::optional<Ms> next;
    std::vector<const Placed*> all;
    for (const Placed& p : scene.elements) all.push_back(&p);
    if (scene.bubble)
        for (const Placed& p : scene.bubble->elements) all.push_back(&p);
    for (const Placed* item : all) {
        const Placed& p = *item;
        if (!p.ticking || !p.ticking->playing || p.ticking->rate <= 0) continue;
        long long position = p.ticking->positionAt(scene.now).count();
        if (p.ticking->duration > Ms{ 0 } && position >= p.ticking->duration.count()) continue;
        // Playback ms until the shown second changes, for elapsed or remaining labels.
        long long remaining = p.ticking->duration.count() - position;
        long long untilElapsed = 1000 - position % 1000;
        long long untilRemaining = remaining > 0 ? remaining % 1000 + 1 : untilElapsed;
        long long playbackMs = std::min(untilElapsed, untilRemaining);
        Ms wallMs{ static_cast<long long>(std::ceil(static_cast<double>(playbackMs) / p.ticking->rate)) };
        Ms at = scene.now + wallMs;
        if (!next || at < *next) next = at;
    }
    return next;
}

} // namespace cup
