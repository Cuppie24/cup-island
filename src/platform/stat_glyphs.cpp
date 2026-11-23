#include "platform/stat_glyphs.h"

#include "platform/text_format.h"

#include <iterator>

namespace cup::platform {
namespace {

// A glyph's strokes on the design's grid: kGrid units square, lines
// kStroke thick with round ends, as the card's mockup drew them. The
// numbers are the shapes themselves, not sizes to tune: the glyph is scaled
// as a whole to the size asked for.
constexpr float kGrid = 18;
constexpr float kStroke = 1.5f;

enum class Stroke { Line, Box, Ring };
struct Part {
    Stroke kind;
    float a, b, c, d; // Line: from (a, b) to (c, d). Box: at (a, b), c wide, d tall. Ring: centre (a, b), radius c
    float corner = 0; // Box: its corners' radius
};

// A square body with two pins on each side.
const Part kChip[] = { { Stroke::Box, 5, 5, 8, 8, 1.6f },
                       { Stroke::Line, 7.4f, 2.6f, 7.4f, 5 },   { Stroke::Line, 10.6f, 2.6f, 10.6f, 5 },
                       { Stroke::Line, 7.4f, 13, 7.4f, 15.4f }, { Stroke::Line, 10.6f, 13, 10.6f, 15.4f },
                       { Stroke::Line, 2.6f, 7.4f, 5, 7.4f },   { Stroke::Line, 2.6f, 10.6f, 5, 10.6f },
                       { Stroke::Line, 13, 7.4f, 15.4f, 7.4f }, { Stroke::Line, 13, 10.6f, 15.4f, 10.6f } };
// A card with its fan, on two feet.
const Part kGraphicsCard[] = { { Stroke::Box, 2.4f, 5, 13.2f, 7.4f, 1.6f }, { Stroke::Ring, 11, 8.7f, 1.9f, 0 },
                               { Stroke::Line, 5, 12.4f, 5, 14.4f },        { Stroke::Line, 8, 12.4f, 8, 14.4f },
                               { Stroke::Line, 5.2f, 7.6f, 5.2f, 9.8f } };
// A memory module: its board, its contacts under it, three chips on it.
const Part kMemory[] = { { Stroke::Box, 2.2f, 5.6f, 13.6f, 5.6f, 1.2f },
                         { Stroke::Line, 5, 11.2f, 5, 13.2f },   { Stroke::Line, 8, 11.2f, 8, 13.2f },
                         { Stroke::Line, 11, 11.2f, 11, 13.2f }, { Stroke::Line, 14, 11.2f, 14, 13.2f },
                         { Stroke::Line, 5.4f, 7.6f, 5.4f, 9.2f }, { Stroke::Line, 9, 7.6f, 9, 9.2f },
                         { Stroke::Line, 12.6f, 7.6f, 12.6f, 9.2f } };
// An arrow up beside an arrow down.
const Part kNetwork[] = { { Stroke::Line, 6, 14.4f, 6, 4 },   { Stroke::Line, 3.4f, 6.6f, 6, 4 },       { Stroke::Line, 6, 4, 8.6f, 6.6f },
                          { Stroke::Line, 12, 3.6f, 12, 14 }, { Stroke::Line, 9.4f, 11.4f, 12, 14 },    { Stroke::Line, 12, 14, 14.6f, 11.4f } };

void paintParts(ID2D1DeviceContext* dc, const Part* parts, std::size_t count, float fontSize, Vec2 size, const Color& color)
{
    float scale = fontSize / kGrid, width = kStroke * scale;
    float left = (size.x - fontSize) / 2, top = (size.y - fontSize) / 2; // centred, like a font's glyph
    auto x = [&](float gridX) { return left + gridX * scale; };
    auto y = [&](float gridY) { return top + gridY * scale; };
    auto brush = brushOf(dc, color);
    // Round ends and corners; without the style (it can't be made), Direct2D's plain stroke.
    winrt::com_ptr<ID2D1Factory> factory;
    dc->GetFactory(factory.put());
    winrt::com_ptr<ID2D1StrokeStyle> rounded;
    auto props = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.0f,
                                             D2D1_DASH_STYLE_SOLID);
    if (factory) factory->CreateStrokeStyle(props, nullptr, 0, rounded.put());
    for (std::size_t i = 0; i < count; ++i) {
        const Part& p = parts[i];
        switch (p.kind) {
        case Stroke::Line: dc->DrawLine(D2D1::Point2F(x(p.a), y(p.b)), D2D1::Point2F(x(p.c), y(p.d)), brush.get(), width, rounded.get()); break;
        case Stroke::Box: {
            D2D1_ROUNDED_RECT box{ D2D1::RectF(x(p.a), y(p.b), x(p.a + p.c), y(p.b + p.d)), p.corner * scale, p.corner * scale };
            dc->DrawRoundedRectangle(box, brush.get(), width, rounded.get());
            break;
        }
        case Stroke::Ring: dc->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x(p.a), y(p.b)), p.c * scale, p.c * scale), brush.get(), width); break;
        }
    }
}

} // namespace

bool paintDrawnGlyph(ID2D1DeviceContext* dc, Glyph glyph, float fontSize, Vec2 size, const Color& color)
{
    switch (glyph) {
    case Glyph::Chip: paintParts(dc, kChip, std::size(kChip), fontSize, size, color); return true;
    case Glyph::GraphicsCard: paintParts(dc, kGraphicsCard, std::size(kGraphicsCard), fontSize, size, color); return true;
    case Glyph::Memory: paintParts(dc, kMemory, std::size(kMemory), fontSize, size, color); return true;
    case Glyph::Network: paintParts(dc, kNetwork, std::size(kNetwork), fontSize, size, color); return true;
    default: return false;
    }
}

} // namespace cup::platform
