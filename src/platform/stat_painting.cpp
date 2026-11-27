#include "platform/stat_painting.h"

#include "core/item_metrics.h"
#include "core/stat_parts.h"
#include "core/tokens.h"
#include "platform/element_painting.h"
#include "platform/text_format.h"

#include <algorithm>
#include <cmath>
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

Color fillOf(bool alert, const Color& live) { return alert ? tokens().dangerColor : live; }

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

// Names and units quiet, their numbers bright (decision 068).
Text quiet(const std::string& value, TextStyle style = TextStyle::Caption)
{
    return Text{ .value = value, .style = style, .brightDigits = true };
}

// A level: its track, and its fill from the left, round at both ends.
void paintLevel(ID2D1DeviceContext* dc, Rect bar, float fill, const Color& ink)
{
    if (bar.width <= 0 || bar.height <= 0) return;
    float r = bar.height / 2;
    dc->FillRoundedRectangle(roundedOf(bar, r), brushOf(dc, tokens().trackColor).get());
    if (fill > 0) dc->FillRoundedRectangle(roundedOf({ bar.x, bar.y, std::min(fill, bar.width), bar.height }, r), brushOf(dc, ink).get());
}

void paintStatTile(ID2D1DeviceContext* dc, IDWriteFactory* dw, const StatTile& tile, Vec2 size, float radius, const Color& live)
{
    const Tokens& t = tokens();
    Rect frame{ 0, 0, size.x, size.y };
    MeasureText measure = [dw](const Text& text) { return measureText(dw, text); };
    StatTileParts parts = statTileParts(tile, frame, measure);
    dc->FillRoundedRectangle(roundedOf(frame, radius), brushOf(dc, t.tileColor).get());
    glyphIn(dc, dw, tile.glyph, std::min(t.iconSize, parts.glyph.height), parts.glyph, t.captionTextColor); // quiet, like its caption
    textIn(dc, dw, Text{ .value = tile.caption, .style = TextStyle::Caption }, parts.caption);
    Text aside = quiet(tile.large ? tile.detail : tile.value);
    aside.alignEnd = true;
    textIn(dc, dw, aside, parts.aside);
    if (tile.large) textIn(dc, dw, quiet(tile.value, TextStyle::Stat), parts.value); // its "%" quieter than its number
    paintLevel(dc, parts.bar, parts.fill, fillOf(tile.alert, live));
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

void paintBatteryTile(ID2D1DeviceContext* dc, IDWriteFactory* dw, const BatteryTile& tile, Vec2 size, float radius, const Color& live)
{
    const Tokens& t = tokens();
    Rect frame{ 0, 0, size.x, size.y };
    MeasureText measure = [dw](const Text& text) { return measureText(dw, text); };
    BatteryTileParts parts = batteryTileParts(tile, frame, measure);
    dc->FillRoundedRectangle(roundedOf(frame, radius), brushOf(dc, t.tileColor).get());
    textIn(dc, dw, Text{ .value = tile.note, .style = TextStyle::Caption, .centred = true }, parts.note); // "No devices connected", "+2 more"
    for (std::size_t i = 0; i < parts.rows.size() && i < tile.rows.size(); ++i) {
        const BatteryRow& row = tile.rows[i];
        const BatteryRowParts& at = parts.rows[i];
        if (parts.alone) {
            // One device with the tile to itself: its glyph in a circle, its name in the title's colour, its percent big.
            float r = at.bubble.width / 2;
            dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(at.bubble.x + r, at.bubble.y + r), r, r), brushOf(dc, t.tileColor).get());
            glyphIn(dc, dw, row.glyph, at.glyph.height, at.glyph, t.iconColor);
            Text name{ .value = row.name, .style = TextStyle::Body };
            name.bright = { { 0, row.name.size() } };
            textIn(dc, dw, name, at.name);
            textIn(dc, dw, quiet(row.value, TextStyle::CardTitle), at.value);
        } else {
            glyphIn(dc, dw, row.glyph, std::min(t.iconSize, at.glyph.height), at.glyph, t.iconColor);
            textIn(dc, dw, Text{ .value = row.name, .style = TextStyle::Caption }, at.name);
            Text value = quiet(row.value);
            value.alignEnd = true;
            textIn(dc, dw, value, at.value);
        }
        paintBatteryShape(dc, at.battery, row.alert, live);
    }
}

} // namespace

bool paintStat(ID2D1DeviceContext* dc, IDWriteFactory* dw, const Item& item, Vec2 size, float radius, const Color& live)
{
    if (auto* tile = std::get_if<StatTile>(&item)) paintStatTile(dc, dw, *tile, size, radius, live);
    else if (auto* batteries = std::get_if<BatteryTile>(&item)) paintBatteryTile(dc, dw, *batteries, size, radius, live);
    else return false;
    return true;
}

} // namespace cup::platform
