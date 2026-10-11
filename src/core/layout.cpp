#include "core/layout.h"

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
};

// Expanded with several activities: the host's switcher column on the right (PS §3.1).
bool hasSwitcher(const IslandState& s, Mode mode) { return mode == Mode::Expanded && s.activities.size() > 1; }

// Where each slot sits inside the pill (PS §5.1). Leading squares are pushed
// in horizontally a little more, clear of the rounded bottom corners.
std::vector<SlotFrame> slotFrames(Mode mode, Vec2 size, const Content& c, bool switcher)
{
    const Tokens& t = tokens();
    switch (mode) {
    case Mode::Compact:
    case Mode::Split: {
        float inset = t.compactInset, side = size.y - 2 * inset, x = 2 * inset;
        return { { "compactLeading", &c.compactLeading, { x, inset, side, side } },
                 { "compactTrailing", &c.compactTrailing, { size.x - x - side, inset, side, side } } };
    }
    case Mode::Alert: {
        float p = t.alertPadding, side = size.y - 2 * p, x = 2 * p, centerX = x + side + t.slotGap;
        return { { "alertLeading", &c.alertLeading, { x, p, side, side } },
                 { "alertCenter", &c.alertCenter, { centerX, p, size.x - centerX - x, side } } };
    }
    case Mode::Expanded: {
        float p = t.expandedPadding, art = t.expandedArtSize, centerX = p + art + t.slotGap, bottomY = p + art + t.slotGap;
        float reserved = switcher ? t.buttonSize + t.itemSpacing : 0;
        return { { "expandedLeading", &c.expandedLeading, { p, p, art, art } },
                 { "expandedCenter", &c.expandedCenter, { centerX, p, size.x - centerX - p - reserved, art } },
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

void placeSlot(const SlotFrame& slot, const Source& source, Ms now, std::vector<Placed>& out)
{
    const Tokens& t = tokens();
    const Slot& rows = *slot.slot;

    std::vector<float> rowHeights;
    float total = 0;
    for (const Row& r : rows) {
        float h = 0;
        for (const Item& item : r.items) h = std::max(h, heightOf(item, slot.frame));
        rowHeights.push_back(h);
        total += h;
    }
    total += t.rowSpacing * static_cast<float>(rows.empty() ? 0 : rows.size() - 1);

    float y = slot.frame.y + (slot.frame.height - total) / 2; // centred vertically
    for (size_t r = 0; r < rows.size(); ++r) {
        const Row& row = rows[r];
        float rowHeight = rowHeights[r];

        float fixed = 0;
        int stretchCount = 0;
        for (const Item& item : row.items) {
            if (auto w = fixedWidthOf(item, heightOf(item, slot.frame))) fixed += *w;
            else ++stretchCount;
        }
        float spacing = t.itemSpacing * static_cast<float>(row.items.empty() ? 0 : row.items.size() - 1);
        float spare = std::max(0.0f, slot.frame.width - fixed - spacing);
        float stretchWidth = stretchCount > 0 ? spare / static_cast<float>(stretchCount) : 0;

        float x = slot.frame.x;
        if (stretchCount == 0 && row.align == Align::Center) x += spare / 2;
        if (stretchCount == 0 && row.align == Align::End) x += spare;

        for (size_t i = 0; i < row.items.size(); ++i) {
            const Item& item = row.items[i];
            float h = heightOf(item, slot.frame);
            float w = fixedWidthOf(item, h).value_or(stretchWidth);
            Placed placed;
            placed.key = std::string(slot.name) + "/" + std::to_string(r) + "/" + std::to_string(i);
            placed.item = resolve(item, now);
            placed.frame = { x, y + (rowHeight - h) / 2, w, h };
            placed.providerId = source.providerId;
            placed.activityId = source.activityId;
            if (auto* label = std::get_if<TimeLabel>(&item)) placed.ticking = label->timeline;
            out.push_back(std::move(placed));
            x += w + t.itemSpacing;
        }
        y += rowHeight + t.rowSpacing;
    }
}

// The switcher: "next activity" on top, and "show in the island" below it
// unless this activity already has the pill (decision 006). Host-owned, so
// no provider id: hitTest's actions for these map to island events.
void placeSwitcher(const IslandState& s, Vec2 size, std::vector<Placed>& out)
{
    const Tokens& t = tokens();
    float x = size.x - t.expandedPadding - t.buttonSize, top = t.expandedPadding;
    auto place = [&](const char* key, Button button, float y) {
        out.push_back({ .key = key, .item = button, .frame = { x, y, t.buttonSize, t.buttonSize }, .activityId = s.expandedId });
    };
    place("switcher/next", { Glyph::NextActivity, kIslandNext }, top);
    if (s.activities.front().id != s.expandedId)
        place("switcher/promote", { Glyph::Pin, kIslandPromote }, top + t.expandedArtSize - t.buttonSize);
}

} // namespace

Scene layout(const IslandState& state, Ms now)
{
    Scene scene;
    scene.now = now;
    scene.mode = mode(state);
    scene.size = sizeFor(scene.mode);
    scene.offset = { (tokens().canvas.x - scene.size.x) / 2, 0 };
    scene.showBubble = scene.mode == Mode::Split;

    Source source = sourceFor(state, scene.mode);
    if (!source.content) return scene;
    scene.label = source.content->label;
    bool switcher = hasSwitcher(state, scene.mode);
    for (const SlotFrame& slot : slotFrames(scene.mode, scene.size, *source.content, switcher))
        placeSlot(slot, source, now, scene.elements);
    if (switcher) placeSwitcher(state, scene.size, scene.elements);
    return scene;
}

std::optional<Ms> sceneDeadline(const Scene& scene)
{
    std::optional<Ms> next;
    for (const Placed& p : scene.elements) {
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
