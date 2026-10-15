#include "platform/element_art.h"

#include "platform/element_parts.h"

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

constexpr float kGlyphRestScale = 0.6f; // hidden glyph's size: it grows out of the art's middle

wuc::SpriteVisual surfaceVisual(Compositor const& c, CompositionDrawingSurface const& surface, Vec2 frame)
{
    auto sprite = c.CreateSpriteVisual();
    sprite.Brush(c.CreateSurfaceBrush(surface));
    sprite.Size(toFloat2(frame));
    return sprite;
}

} // namespace

ArtParts createActionArt(Compositor const& c, Vec2 frame, float cornerRadius, CompositionDrawingSurface const& art,
                         CompositionDrawingSurface const& glyph)
{
    // The dim follows the art's own rounded corners (the nested radius).
    auto shape = c.CreateRoundedRectangleGeometry();
    shape.Size(toFloat2(frame));
    shape.CornerRadius({ cornerRadius, cornerRadius });
    ArtParts parts;
    parts.dim = c.CreateColorBrush(toUiColor(tokens().artHoverColor));
    auto dim = c.CreateSpriteShape(shape);
    dim.FillBrush(parts.dim);
    auto dimVisual = c.CreateShapeVisual();
    dimVisual.Size(toFloat2(frame));
    dimVisual.Shapes().Append(dim);

    auto glyphVisual = surfaceVisual(c, glyph, frame);
    glyphVisual.CenterPoint({ frame.x / 2, frame.y / 2, 0 });
    glyphVisual.Scale({ kGlyphRestScale, kGlyphRestScale, 1 });
    parts.glyph = glyphVisual;

    auto overlay = c.CreateContainerVisual();
    overlay.Size(toFloat2(frame));
    overlay.Opacity(0.0f);
    overlay.Children().InsertAtTop(dimVisual);
    overlay.Children().InsertAtTop(glyphVisual);
    parts.overlay = overlay;

    parts.visual = c.CreateContainerVisual();
    parts.visual.Children().InsertAtTop(surfaceVisual(c, art, frame));
    parts.visual.Children().InsertAtTop(overlay);
    return parts;
}

void showArtOverlay(Compositor const& c, const ArtParts& parts, bool on)
{
    const Tokens& t = tokens();
    fadeTo(c, parts.overlay, on ? 1.0f : 0.0f, t.hoverFadeDuration);
    if (on) playScale(c, parts.glyph, 1.0f, t.pressReleaseDuration, t.pressReleaseEase); // springs, slightly past full size
    else playScale(c, parts.glyph, kGlyphRestScale, t.hoverFadeDuration, t.morphEase);
}

} // namespace cup::platform
