#include "core/stat_parts.h"

#include "core/item_metrics.h"
#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

float unit(float level) { return std::isfinite(level) ? std::clamp(level, 0.0f, 1.0f) : 0.0f; }

float widthOf(const std::string& value, const MeasureText& measure)
{
    if (value.empty()) return 0;
    Text text{ value, TextStyle::Caption };
    return (measure ? measure(text) : estimateTextWidth(text)) + 1; // +1: rounding never trips the "…"
}

} // namespace

float graphHeight(GraphSize size)
{
    const Tokens& t = tokens();
    switch (size) {
    case GraphSize::Tall: return t.graphTallHeight;
    case GraphSize::Low: return t.graphLowHeight;
    case GraphSize::Strip: break;
    }
    return t.graphStripHeight;
}

GraphParts graphParts(const Graph& graph, Rect frame)
{
    const Tokens& t = tokens();
    if (!graph.tile) return { frame, {} };
    // The plot under the texts, to the tile's bottom and sides: the tile's rounded shape clips it.
    float top = std::min(graph.size == GraphSize::Tall ? t.graphTallPlotTop : t.graphLowPlotTop, frame.height);
    float line = std::min(lineHeight(graph.valueLarge ? TextStyle::CardTitle : TextStyle::Body), std::max(0.0f, top - t.graphTextTop));
    return { { frame.x, frame.y + top, frame.width, frame.height - top },
             { frame.x + t.graphInset, frame.y + t.graphTextTop, std::max(0.0f, frame.width - 2 * t.graphInset), line } };
}

std::vector<std::vector<Vec2>> graphLines(const Graph& graph, Rect plot)
{
    std::vector<std::vector<Vec2>> runs;
    std::size_t count = graph.samples.size(), span = std::max(graph.capacity, count);
    if (count == 0 || span < 2) return runs;
    float step = plot.width / static_cast<float>(span - 1);
    bool open = false;
    for (std::size_t i = 0; i < count; ++i) {
        float sample = graph.samples[i];
        if (!(sample >= 0)) { // below 0, or not a number: no reading
            open = false;
            continue;
        }
        if (!open) runs.emplace_back();
        open = true;
        // The newest at the right edge, whatever came before it.
        float x = plot.x + step * static_cast<float>(span - count + i);
        runs.back().push_back({ x, plot.y + plot.height * (1 - unit(sample)) });
    }
    return runs;
}

std::optional<float> graphLimitY(const Graph& graph, Rect plot)
{
    if (!graph.limit) return std::nullopt;
    return plot.y + plot.height * (1 - unit(*graph.limit));
}

std::vector<LevelFrame> levelFrames(const Levels& levels, Rect frame)
{
    const Tokens& t = tokens();
    std::vector<LevelFrame> frames;
    std::size_t count = levels.bars.size();
    if (count == 0) return frames;
    float n = static_cast<float>(count);
    float gap = t.levelsGap * (n - 1) < frame.width / 2 ? t.levelsGap : 0; // many cores in a narrow strip: bars, not gaps
    float width = std::max(0.0f, (frame.width - gap * (n - 1)) / n);
    for (std::size_t i = 0; i < count; ++i) {
        Rect bar{ frame.x + (width + gap) * static_cast<float>(i), frame.y, width, frame.height };
        float filled = bar.height * unit(levels.bars[i].level);
        frames.push_back({ bar, { bar.x, bar.y + bar.height - filled, bar.width, filled } });
    }
    return frames;
}

std::optional<std::size_t> levelAt(const Levels& levels, Rect frame, Vec2 point)
{
    if (!frame.contains(point) || levels.bars.empty()) return std::nullopt;
    // By its share of the width, so the gaps between bars belong to one too.
    float share = frame.width / static_cast<float>(levels.bars.size());
    auto index = static_cast<std::size_t>(std::max(0.0f, (point.x - frame.x) / share));
    return std::min(index, levels.bars.size() - 1);
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

namespace {

float shapeWidth()
{
    const Tokens& t = tokens();
    return t.batterySize.x + t.batteryStroke + t.batteryCapWidth;
}

} // namespace

float batteryWidth(const Battery& battery, const MeasureText& measure)
{
    const Tokens& t = tokens();
    float width = shapeWidth();
    if (!battery.label.empty()) width += widthOf(battery.label, measure) + t.batteryTextGap;
    if (!battery.value.empty()) width += widthOf(battery.value, measure) + t.batteryTextGap;
    return width;
}

BatteryParts batteryParts(const Battery& battery, Rect frame, const MeasureText& measure)
{
    const Tokens& t = tokens();
    float labelWidth = widthOf(battery.label, measure), valueWidth = widthOf(battery.value, measure);
    float left = frame.x + (labelWidth > 0 ? labelWidth + t.batteryTextGap : 0);
    BatteryParts parts = batteryShape(battery.level, left, frame.y + frame.height / 2);
    parts.label = { frame.x, frame.y, labelWidth, frame.height };
    parts.value = { left + shapeWidth() + (valueWidth > 0 ? t.batteryTextGap : 0), frame.y, valueWidth, frame.height };
    return parts;
}

LevelTileParts levelTileParts(const LevelTile& tile, Rect frame, const MeasureText& measure)
{
    const Tokens& t = tokens();
    float line = lineHeight(TextStyle::Caption);
    float height = 2 * line + t.progressHeight + 2 * t.tileRowGap;
    float x = frame.x + t.graphInset, width = std::max(0.0f, frame.width - 2 * t.graphInset);
    float y = frame.y + (frame.height - height) / 2;
    LevelTileParts parts;
    parts.glyph = { x, y, line, line };
    float valueWidth = std::min(widthOf(tile.value, measure), width);
    parts.value = { x + width - valueWidth, y, valueWidth, line };
    float captionX = x + line + t.itemSpacing;
    parts.caption = { captionX, y, std::max(0.0f, parts.value.x - t.itemSpacing - captionX), line };
    parts.bar = { x, y + line + t.tileRowGap, width, t.progressHeight };
    parts.fill = width * unit(tile.level);
    parts.detail = { x, parts.bar.y + t.progressHeight + t.tileRowGap, width, line };
    return parts;
}

std::vector<BatteryCellParts> batteryTileParts(const BatteryTile& tile, Rect frame)
{
    const Tokens& t = tokens();
    std::vector<BatteryCellParts> cells;
    if (tile.cells.empty()) return cells;
    float line = lineHeight(TextStyle::Caption);
    float height = t.iconSize + t.batterySize.y + line + 2 * t.tileRowGap;
    float top = frame.y + (frame.height - height) / 2;
    float share = frame.width / static_cast<float>(tile.cells.size());
    for (std::size_t i = 0; i < tile.cells.size(); ++i) {
        float left = frame.x + share * static_cast<float>(i), middle = left + share / 2;
        BatteryCellParts cell;
        cell.glyph = { middle - t.iconSize / 2, top, t.iconSize, t.iconSize };
        float batteryTop = top + t.iconSize + t.tileRowGap;
        cell.battery = batteryShape(tile.cells[i].level, middle - shapeWidth() / 2, batteryTop + t.batterySize.y / 2);
        cell.value = { left, batteryTop + t.batterySize.y + t.tileRowGap, share, line };
        cells.push_back(cell);
    }
    return cells;
}

std::string tileAction(const Item& item)
{
    if (auto* graph = std::get_if<Graph>(&item)) return graph->actionId;
    if (auto* level = std::get_if<LevelTile>(&item)) return level->actionId;
    if (auto* battery = std::get_if<BatteryTile>(&item)) return battery->actionId;
    return "";
}

float fractionOf(const Progress& progress, Ms now)
{
    return progress.level ? unit(*progress.level) : progress.timeline.fractionAt(now);
}

LevelBarParts levelBarParts(const Progress& progress, Rect frame)
{
    float level = unit(progress.level.value_or(0));
    float second = std::min(unit(progress.second.value_or(0)), 1 - level); // what's left of the bar at most
    float filled = frame.width * level;
    return { { frame.x, frame.y, filled, frame.height }, { frame.x + filled, frame.y, frame.width * second, frame.height } };
}

} // namespace cup
