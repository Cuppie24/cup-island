#include "platform/element_art.h"

#include "platform/element_parts.h"

#include <algorithm>

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
    parts.picture = surfaceVisual(c, art, frame);
    parts.visual.Children().InsertAtTop(parts.picture);
    parts.visual.Children().InsertAtTop(overlay);
    return parts;
}

DropShadow addArtShadow(Compositor const& c, ContainerVisual const& container, Vec2 frame, CompositionDrawingSurface const& shape)
{
    const Tokens& t = tokens();
    auto shadow = c.CreateDropShadow();
    shadow.Mask(c.CreateSurfaceBrush(shape)); // DropShadow.Mask: Windows 10 1607, so every supported Windows (decision 015)
    ArtShadow look = artShadow(t); // its shadow, or its glow while its music plays (decision 169)
    shadow.BlurRadius(look.blur);
    shadow.Offset(toFloat3(look.offset));
    shadow.Color(toUiColor(look.color));
    auto caster = c.CreateSpriteVisual(); // no brush: only its shadow shows
    caster.Size(toFloat2(frame));
    caster.Shadow(shadow);
    container.Children().InsertAtBottom(caster);
    return shadow;
}

void fadeArtShadow(Compositor const& c, DropShadow const& shadow)
{
    ArtShadow look = artShadow(tokens());
    Ms duration = std::max(recolorDuration(tokens()), Ms{ 1 });
    fadeColor(shadow, look.color);
    // Its softness and where it falls change with it: a shadow below, a glow all round.
    auto blur = c.CreateScalarKeyFrameAnimation();
    blur.InsertKeyFrame(1.0f, look.blur);
    blur.Duration(duration);
    shadow.StartAnimation(L"BlurRadius", blur);
    auto offset = c.CreateVector3KeyFrameAnimation();
    offset.InsertKeyFrame(1.0f, toFloat3(look.offset));
    offset.Duration(duration);
    shadow.StartAnimation(L"Offset", offset);
}

void showArtOverlay(Compositor const& c, const ArtParts& parts, bool on)
{
    const Tokens& t = tokens();
    fadeTo(c, parts.overlay, on ? 1.0f : 0.0f, t.hoverFadeDuration);
    if (on) playScale(c, parts.glyph, 1.0f, t.pressReleaseDuration, t.pressReleaseEase); // springs, slightly past full size
    else playScale(c, parts.glyph, kGlyphRestScale, t.hoverFadeDuration, t.morphEase);
}

} // namespace cup::platform
