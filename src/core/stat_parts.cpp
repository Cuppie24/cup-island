#include "core/stat_parts.h"

#include "core/item_metrics.h"
#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

float unit(float level) { return std::isfinite(level) ? std::clamp(level, 0.0f, 1.0f) : 0.0f; }

float widthOf(const std::string& value, TextStyle style, const MeasureText& measure)
{
    if (value.empty()) return 0;
    Text text{ value, style };
    return (measure ? measure(text) : estimateTextWidth(text)) + 1; // +1: rounding never trips the "…"
}

} // namespace

float statTileHeight(const StatTile& tile) { return tile.large ? tokens().statTileHeight : tokens().statMiniHeight; }

StatTileParts statTileParts(const StatTile& tile, Rect frame, const MeasureText& measure)
{
    const Tokens& t = tokens();
    float line = lineHeight(TextStyle::Caption);
    float x = frame.x + t.tileInset, width = std::max(0.0f, frame.width - 2 * t.tileInset);
    StatTileParts parts;
    float top = frame.y + t.tileInset; // a large tile: its header at the top, its bar at the bottom
    if (tile.level) parts.bar = { x, frame.y + frame.height - t.tileInset - t.progressHeight, width, t.progressHeight };
    if (!tile.large) { // a small one: its line, and its bar if it has one, centred together
        float block = line + (tile.level ? t.tileRowGap + t.progressHeight : 0.0f);
        top = frame.y + (frame.height - block) / 2;
        if (tile.level) parts.bar.y = top + line + t.tileRowGap;
    }
    parts.fill = parts.bar.width * unit(tile.level.value_or(0));
    parts.glyph = { x, top, line, line };
    float asideWidth = std::min(width, widthOf(tile.large ? tile.detail : tile.value, TextStyle::Caption, measure));
    parts.aside = { x + width - asideWidth, top, asideWidth, line };
    // The caption gets what's between the glyph and what's at the right, or
    // nothing: "Netw…" says less than the glyph alone.
    float captionX = x + line + t.tileRowGap;
    float room = parts.aside.x - (asideWidth > 0 ? t.itemSpacing : 0.0f) - captionX;
    if (widthOf(tile.caption, TextStyle::Caption, measure) <= room) parts.caption = { captionX, top, room, line };
    if (tile.large) {
        float bottom = tile.level ? parts.bar.y - t.itemSpacing : frame.y + frame.height - t.tileInset;
        parts.value = { x, bottom - t.statValueLineHeight, width, t.statValueLineHeight };
    }
    return parts;
}

float batteryShapeWidth()
{
    const Tokens& t = tokens();
    return t.batterySize.x + t.batteryStroke + t.batteryCapWidth;
}

BatteryParts batteryShape(float level, float left, float middle)
{
    const Tokens& t = tokens();
    BatteryParts parts;
    parts.body = { left, middle - t.batterySize.y / 2, t.batterySize.x, t.batterySize.y };
    float capHeight = t.batterySize.y / 3;
    parts.cap = { left + t.batterySize.x + t.batteryStroke, middle - capHeight / 2, t.batteryCapWidth, capHeight };
    // Inside the outline, a stroke's width clear of it.
    float inset = 2 * t.batteryStroke;
    Rect inner{ left + inset, parts.body.y + inset, std::max(0.0f, t.batterySize.x - 2 * inset), std::max(0.0f, t.batterySize.y - 2 * inset) };
    parts.fill = { inner.x, inner.y, inner.width * unit(level), inner.height };
    // A share of its height, so the shape keeps its look at any scale; the fill's corners nest in the outline's.
    parts.bodyRadius = t.batterySize.y / 3;
    parts.fillRadius = std::max(0.0f, parts.bodyRadius - inset);
    return parts;
}

float batteryTileHeight()
{
    const Tokens& t = tokens();
    return 2 * t.statMiniHeight + t.itemSpacing; // beside two small tiles, one over the other
}

std::size_t batteryTileLines()
{
    const Tokens& t = tokens();
    if (t.batteryRowHeight <= 0) return 0;
    return static_cast<std::size_t>(std::max(0.0f, (batteryTileHeight() - t.tileInset) / t.batteryRowHeight));
}

namespace {

// One device with the tile to itself: its glyph in a circle, then its name over its battery and percent.
BatteryRowParts aloneParts(const BatteryRow& row, Rect frame)
{
    const Tokens& t = tokens();
    BatteryRowParts parts;
    float d = std::min(t.deviceBubbleSize, frame.height), middle = frame.y + frame.height / 2;
    parts.bubble = { frame.x + t.tileInset, middle - d / 2, d, d };
    float icon = std::min(t.largeIconSize, d);
    parts.glyph = { parts.bubble.x + (d - icon) / 2, middle - icon / 2, icon, icon };
    float x = parts.bubble.x + d + t.tileInset, width = std::max(0.0f, frame.x + frame.width - t.tileInset - x);
    float nameLine = lineHeight(TextStyle::Body), valueLine = lineHeight(TextStyle::CardTitle);
    float top = middle - (nameLine + valueLine) / 2;
    parts.name = { x, top, width, nameLine };
    parts.battery = batteryShape(row.level, x, top + nameLine + valueLine / 2);
    float valueX = x + batteryShapeWidth() + t.itemSpacing;
    parts.value = { valueX, top + nameLine, std::max(0.0f, x + width - valueX), valueLine };
    return parts;
}

} // namespace

BatteryTileParts batteryTileParts(const BatteryTile& tile, Rect frame, const MeasureText& measure)
{
    const Tokens& t = tokens();
    BatteryTileParts parts;
    if (tile.rows.empty()) {
        if (!tile.note.empty()) parts.note = frame;
        return parts;
    }
    if (tile.rows.size() == 1 && tile.note.empty()) {
        parts.alone = true;
        parts.rows.push_back(aloneParts(tile.rows[0], frame));
        return parts;
    }
    float x = frame.x + t.tileInset, width = std::max(0.0f, frame.width - 2 * t.tileInset);
    float lines = static_cast<float>(tile.rows.size() + (tile.note.empty() ? 0 : 1));
    float top = frame.y + (frame.height - lines * t.batteryRowHeight) / 2;
    float icon = lineHeight(TextStyle::Caption);
    float valueWidth = std::min(width, widthOf("100%", TextStyle::Caption, measure)); // one column, whatever each reads
    for (std::size_t i = 0; i < tile.rows.size(); ++i) {
        float y = top + t.batteryRowHeight * static_cast<float>(i), middle = y + t.batteryRowHeight / 2;
        BatteryRowParts row;
        row.glyph = { x, middle - icon / 2, icon, icon };
        row.value = { x + width - valueWidth, y, valueWidth, t.batteryRowHeight };
        float batteryX = row.value.x - t.itemSpacing - batteryShapeWidth();
        row.battery = batteryShape(tile.rows[i].level, batteryX, middle);
        float nameX = x + icon + t.itemSpacing;
        row.name = { nameX, y, std::max(0.0f, batteryX - t.itemSpacing - nameX), t.batteryRowHeight };
        parts.rows.push_back(row);
    }
    if (!tile.note.empty()) parts.note = { x, top + t.batteryRowHeight * static_cast<float>(tile.rows.size()), width, t.batteryRowHeight };
    return parts;
}

} // namespace cup
