#include "platform/stat_painting.h"

#include "core/item_metrics.h"
#include "core/stat_parts.h"
#include "core/tokens.h"
#include "platform/element_painting.h"
#include "platform/text_format.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace cup::platform {
namespace {

D2D1_RECT_F boxOf(Rect r) { return D2D1::RectF(r.x, r.y, r.x + r.width, r.y + r.height); }

// Corners never rounder than the shape allows.
D2D1_ROUNDED_RECT roundedOf(Rect r, float radius)
{
    radius = std::clamp(radius, 0.0f, std::max(0.0f, std::min(r.width, r.height) / 2));
    return D2D1_ROUNDED_RECT{ boxOf(r), radius, radius };
}

D2D1_COLOR_F colourOf(const Color& c) { return D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f); }

// `c`, `share` as strong: the area under a line, a bar's second part.
Color faded(Color c, float share)
{
    c.a = static_cast<std::uint8_t>(std::lround(c.a * std::clamp(share, 0.0f, 1.0f)));
    return c;
}

Color fillOf(bool alert, const Color& live) { return alert ? tokens().dangerColor : live; }

winrt::com_ptr<ID2D1Factory> factoryOf(ID2D1DeviceContext* dc)
{
    winrt::com_ptr<ID2D1Factory> factory;
    dc->GetFactory(factory.put());
    return factory;
}

// What's drawn from here to PopLayer stays inside the rounded rect: a plot in
// its tile's corners, a level in its bar's. False (nothing pushed) if the
// shape can't be made; the caller then draws unclipped rather than nothing.
bool pushRoundedClip(ID2D1DeviceContext* dc, Rect r, float radius)
{
    auto factory = factoryOf(dc);
    winrt::com_ptr<ID2D1RoundedRectangleGeometry> shape;
    if (!factory || FAILED(factory->CreateRoundedRectangleGeometry(roundedOf(r, radius), shape.put()))) return false;
    // The render target's own PushLayer: a null layer is allowed since Windows 8, and Direct2D makes one.
    ID2D1RenderTarget* target = dc;
    target->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), shape.get()), nullptr);
    return true;
}

// `text` in `box`, as paintText draws it at a surface's corner. On whole
// pixels: text drawn from a fraction of one is soft.
void textIn(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Text& text, Rect box)
{
    if (text.value.empty() || box.width <= 0 || box.height <= 0) return;
    D2D1_MATRIX_3X2_F was{};
    dc->GetTransform(&was);
    dc->SetTransform(D2D1::Matrix3x2F::Translation(std::round(box.x), std::round(box.y)) * was);
    paintText(dc, dw, text, { box.width, box.height });
    dc->SetTransform(was);
}

void glyphIn(ID2D1DeviceContext* dc, IDWriteFactory* dw, Glyph glyph, float fontSize, Rect box, const Color& color)
{
    if (box.width <= 0 || box.height <= 0) return;
    D2D1_MATRIX_3X2_F was{};
    dc->GetTransform(&was);
    dc->SetTransform(D2D1::Matrix3x2F::Translation(std::round(box.x), std::round(box.y)) * was);
    paintGlyph(dc, dw, glyph, fontSize, { box.width, box.height }, color);
    dc->SetTransform(was);
}

float widthOf(IDWriteFactory* dw, const Text& text)
{
    return text.value.empty() ? 0.0f : std::ceil(measureText(dw, text)) + 1; // +1: rounding never trips the "…"
}

Text quiet(const std::string& value) // names and units quiet, their numbers bright (decision 068)
{
    return Text{ .value = value, .style = TextStyle::Caption, .brightDigits = true };
}

// --- Graph -------------------------------------------------------------------

// A line through `points`; with `floor`, the closed area between it and that height instead.
winrt::com_ptr<ID2D1PathGeometry> pathOf(ID2D1Factory* factory, const std::vector<Vec2>& points, std::optional<float> floor)
{
    winrt::com_ptr<ID2D1PathGeometry> path;
    winrt::com_ptr<ID2D1GeometrySink> sink;
    if (points.empty() || FAILED(factory->CreatePathGeometry(path.put())) || FAILED(path->Open(sink.put()))) return nullptr;
    if (floor) {
        sink->BeginFigure(D2D1::Point2F(points.front().x, *floor), D2D1_FIGURE_BEGIN_FILLED);
        for (const Vec2& p : points) sink->AddLine(D2D1::Point2F(p.x, p.y));
        sink->AddLine(D2D1::Point2F(points.back().x, *floor));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    } else {
        sink->BeginFigure(D2D1::Point2F(points.front().x, points.front().y), D2D1_FIGURE_BEGIN_HOLLOW);
        for (std::size_t i = 1; i < points.size(); ++i) sink->AddLine(D2D1::Point2F(points[i].x, points[i].y));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
    }
    if (FAILED(sink->Close())) return nullptr;
    return path;
}

// The area's fill: the line's colour, strongest under the plot's top, nearly gone at its bottom.
winrt::com_ptr<ID2D1LinearGradientBrush> areaBrush(ID2D1DeviceContext* dc, Rect plot, const Color& ink)
{
    const Tokens& t = tokens();
    D2D1_GRADIENT_STOP stops[2] = { { 0.0f, colourOf(faded(ink, t.graphAreaOpacity)) }, { 1.0f, colourOf(faded(ink, t.graphAreaFadeOpacity)) } };
    winrt::com_ptr<ID2D1GradientStopCollection> ramp;
    winrt::com_ptr<ID2D1LinearGradientBrush> brush;
    // The render target's own two calls, named through it: the device context adds overloads of its own beside them.
    ID2D1RenderTarget* target = dc;
    if (FAILED(target->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, ramp.put()))) return nullptr;
    auto ends = D2D1::LinearGradientBrushProperties(D2D1::Point2F(plot.x, plot.y), D2D1::Point2F(plot.x, plot.y + plot.height));
    if (FAILED(target->CreateLinearGradientBrush(ends, ramp.get(), brush.put()))) return nullptr;
    return brush;
}

void paintPlot(ID2D1DeviceContext* dc, const Graph& graph, Rect plot, const Color& ink)
{
    const Tokens& t = tokens();
    auto factory = factoryOf(dc);
    if (!factory || plot.width <= 0 || plot.height <= 0) return;
    float bottom = plot.y + plot.height;
    auto area = areaBrush(dc, plot, ink);
    auto line = brushOf(dc, ink);
    winrt::com_ptr<ID2D1StrokeStyle> joined, dashed; // null: Direct2D's plain stroke
    auto smooth = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
    factory->CreateStrokeStyle(smooth, nullptr, 0, joined.put());
    for (const std::vector<Vec2>& run : graphLines(graph, plot)) {
        if (run.size() == 1) { // one reading between two gaps: a dot, so it isn't lost
            dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(run[0].x, run[0].y), t.graphLineWidth, t.graphLineWidth), line.get());
            continue;
        }
        if (auto under = pathOf(factory.get(), run, bottom); under && area) dc->FillGeometry(under.get(), area.get());
        if (auto through = pathOf(factory.get(), run, std::nullopt)) dc->DrawGeometry(through.get(), line.get(), t.graphLineWidth, joined.get());
    }
    if (auto y = graphLimitY(graph, plot)) { // where the reading turns high: a thin dashed line, whatever the graph's own colour
        auto dashes = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_MITER, 10.0f,
                                                  D2D1_DASH_STYLE_DASH);
        factory->CreateStrokeStyle(dashes, nullptr, 0, dashed.put());
        float at = std::round(*y) + t.dividerThickness / 2; // on a pixel row: a hairline between two is a blur
        dc->DrawLine(D2D1::Point2F(plot.x, at), D2D1::Point2F(plot.x + plot.width, at), brushOf(dc, t.dangerColor).get(), t.dividerThickness, dashed.get());
    }
}

// A tile's texts over its plot: the caption from the left with its detail beside it, the value at the right.
void paintGraphTexts(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Graph& graph, Rect line)
{
    const Tokens& t = tokens();
    Text value{ .value = graph.value, .style = graph.valueLarge ? TextStyle::CardTitle : TextStyle::Body, .alignEnd = true, .brightDigits = true };
    float valueWidth = std::min(line.width, widthOf(dw, value));
    textIn(dc, dw, value, { line.x + line.width - valueWidth, line.y, valueWidth, line.height });
    // The value keeps its width; what's left of it is the caption's, which ends in "…" if it must.
    float room = std::max(0.0f, line.width - valueWidth - (valueWidth > 0 ? t.itemSpacing : 0.0f));
    Text caption = quiet(graph.caption);
    if (graph.captionCentred) {
        caption.centred = true;
        textIn(dc, dw, caption, { line.x, line.y, room, line.height });
        return;
    }
    float captionWidth = std::min(room, widthOf(dw, caption));
    textIn(dc, dw, caption, { line.x, line.y, captionWidth, line.height });
    float detailLeft = captionWidth + (captionWidth > 0 ? t.itemSpacing : 0.0f);
    textIn(dc, dw, quiet(graph.detail), { line.x + detailLeft, line.y, room - detailLeft, line.height });
}

void paintGraph(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Graph& graph, Vec2 size, float radius, const Color& live)
{
    Rect frame{ 0, 0, size.x, size.y };
    GraphParts parts = graphParts(graph, frame);
    bool clipped = false;
    if (graph.tile) {
        dc->FillRoundedRectangle(roundedOf(frame, radius), brushOf(dc, tokens().tileColor).get());
        clipped = pushRoundedClip(dc, frame, radius); // the plot reaches the tile's bottom corners
    }
    paintPlot(dc, graph, parts.plot, fillOf(graph.alert, live));
    if (clipped) dc->PopLayer();
    if (graph.tile) paintGraphTexts(dc, dw, graph, parts.texts);
}

// --- Levels, Battery ---------------------------------------------------------

void paintLevels(ID2D1DeviceContext* dc, const Levels& levels, Vec2 size, const Color& live)
{
    const Tokens& t = tokens();
    auto track = brushOf(dc, t.tileColor);
    std::vector<LevelFrame> frames = levelFrames(levels, { 0, 0, size.x, size.y });
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const LevelFrame& f = frames[i];
        dc->FillRoundedRectangle(roundedOf(f.bar, t.levelsRadius), track.get());
        if (f.fill.height <= 0) continue;
        // Flat on top, the bar's corners at its bottom: the fill cut by the bar's shape.
        bool clipped = pushRoundedClip(dc, f.bar, t.levelsRadius);
        dc->FillRectangle(boxOf(f.fill), brushOf(dc, fillOf(levels.bars[i].alert, live)).get());
        if (clipped) dc->PopLayer();
    }
}

// The outline, its nub and the charge in it.
void paintBatteryShape(ID2D1DeviceContext* dc, const BatteryParts& parts, bool alert, const Color& live)
{
    const Tokens& t = tokens();
    auto outline = brushOf(dc, t.captionTextColor);
    float w = t.batteryStroke, in = w / 2; // a stroke is centred on its path: half a width in keeps it inside the body
    Rect path{ parts.body.x + in, parts.body.y + in, std::max(0.0f, parts.body.width - w), std::max(0.0f, parts.body.height - w) };
    dc->DrawRoundedRectangle(roundedOf(path, parts.bodyRadius - in), outline.get(), w);
    dc->FillRoundedRectangle(roundedOf(parts.cap, parts.cap.width / 2), outline.get());
    if (parts.fill.width > 0) dc->FillRoundedRectangle(roundedOf(parts.fill, parts.fillRadius), brushOf(dc, fillOf(alert, live)).get());
}

void paintBattery(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Battery& battery, Vec2 size, const Color& live)
{
    MeasureText measure = [dw](const Text& text) { return measureText(dw, text); };
    BatteryParts parts = batteryParts(battery, { 0, 0, size.x, size.y }, measure);
    paintBatteryShape(dc, parts, battery.alert, live);
    textIn(dc, dw, Text{ .value = battery.label, .style = TextStyle::Caption }, parts.label);
    textIn(dc, dw, quiet(battery.value), parts.value);
}

// --- The overview's tiles, a bar with a level --------------------------------

void paintLevelTile(ID2D1DeviceContext* dc, IDWriteFactory* dw, const LevelTile& tile, Vec2 size, float radius, const Color& live)
{
    const Tokens& t = tokens();
    Rect frame{ 0, 0, size.x, size.y };
    MeasureText measure = [dw](const Text& text) { return measureText(dw, text); };
    LevelTileParts parts = levelTileParts(tile, frame, measure);
    dc->FillRoundedRectangle(roundedOf(frame, radius), brushOf(dc, t.tileColor).get());
    glyphIn(dc, dw, tile.glyph, std::min(t.iconSize, parts.glyph.height), parts.glyph, t.captionTextColor); // quiet, like its caption
    textIn(dc, dw, Text{ .value = tile.caption, .style = TextStyle::Caption }, parts.caption);
    Text value = quiet(tile.value);
    value.alignEnd = true;
    textIn(dc, dw, value, parts.value);
    float r = parts.bar.height / 2;
    dc->FillRoundedRectangle(roundedOf(parts.bar, r), brushOf(dc, t.trackColor).get());
    if (parts.fill > 0)
        dc->FillRoundedRectangle(roundedOf({ parts.bar.x, parts.bar.y, parts.fill, parts.bar.height }, r), brushOf(dc, fillOf(tile.alert, live)).get());
    textIn(dc, dw, quiet(tile.detail), parts.detail);
}

void paintBatteryTile(ID2D1DeviceContext* dc, IDWriteFactory* dw, const BatteryTile& tile, Vec2 size, float radius, const Color& live)
{
    const Tokens& t = tokens();
    Rect frame{ 0, 0, size.x, size.y };
    dc->FillRoundedRectangle(roundedOf(frame, radius), brushOf(dc, t.tileColor).get());
    if (tile.cells.empty()) { // nothing connected: it says so, in the tile's middle
        textIn(dc, dw, Text{ .value = tile.empty, .style = TextStyle::Caption, .centred = true }, frame);
        return;
    }
    std::vector<BatteryCellParts> cells = batteryTileParts(tile, frame);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const BatteryCell& cell = tile.cells[i];
        glyphIn(dc, dw, cell.glyph, t.iconSize, cells[i].glyph, t.iconColor);
        paintBatteryShape(dc, cells[i].battery, cell.alert, live);
        Text value = quiet(cell.value);
        value.centred = true;
        textIn(dc, dw, value, cells[i].value);
    }
}

void paintLevelBar(ID2D1DeviceContext* dc, const Progress& bar, Vec2 size, const Color& live)
{
    const Tokens& t = tokens();
    Rect frame{ 0, 0, size.x, size.y };
    LevelBarParts parts = levelBarParts(bar, frame);
    float r = size.y / 2;
    Color ink = fillOf(bar.alert, live);
    dc->FillRoundedRectangle(roundedOf(frame, r), brushOf(dc, t.trackColor).get());
    if (!bar.second) { // one part: round at both ends, like a bar that follows a timeline
        if (parts.fill.width > 0) dc->FillRoundedRectangle(roundedOf(parts.fill, r), brushOf(dc, ink).get());
        return;
    }
    // Two parts meet flat, and only the bar's own ends are round: both cut by its shape.
    bool clipped = pushRoundedClip(dc, frame, r);
    dc->FillRectangle(boxOf(parts.fill), brushOf(dc, ink).get());
    dc->FillRectangle(boxOf(parts.second), brushOf(dc, faded(ink, t.progressSecondOpacity)).get());
    if (clipped) dc->PopLayer();
}

} // namespace

bool paintedLevel(const Item& item)
{
    auto* progress = std::get_if<Progress>(&item);
    return progress && progress->level && progress->shape == ProgressShape::Bar;
}

bool paintStat(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Item& item, Vec2 size, float radius, const Color& live)
{
    if (auto* graph = std::get_if<Graph>(&item)) paintGraph(dc, dw, *graph, size, radius, live);
    else if (auto* levels = std::get_if<Levels>(&item)) paintLevels(dc, *levels, size, live);
    else if (auto* battery = std::get_if<Battery>(&item)) paintBattery(dc, dw, *battery, size, live);
    else if (auto* level = std::get_if<LevelTile>(&item)) paintLevelTile(dc, dw, *level, size, radius, live);
    else if (auto* batteries = std::get_if<BatteryTile>(&item)) paintBatteryTile(dc, dw, *batteries, size, radius, live);
    else if (paintedLevel(item)) paintLevelBar(dc, std::get<Progress>(item), size, live);
    else return false;
    return true;
}

} // namespace cup::platform
