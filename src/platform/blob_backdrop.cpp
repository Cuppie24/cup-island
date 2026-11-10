// What a Blob draws besides its body: the outline twin under it, the shadow
// it casts (decision 018), and as glass, the blurred desktop between the
// two and a rim on the body (decision 089).

#include "platform/blob.h"

#include "platform/blob_shape.h"
#include "platform/element_parts.h"
#include "platform/glass.h"

#include <winrt/Windows.Foundation.Metadata.h>

#include <string>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

constexpr auto bind = bindToBlob;

// `visual` covers the body and its ears (decision 025), through every morph,
// press and drag: the shadow's caster and the glass, both cut to the
// outline's snapshot, which is as wide.
void coverBodyAndEars(Compositor const& c, Visual const& visual, ShapeVisual const& body, CompositionPropertySet const& shape)
{
    bind(c, visual, L"Size", L"Vector2(pill.Size.X + 2 * " + kEarSize + L", pill.Size.Y)", body, shape);
    bind(c, visual, L"Offset", L"pill.Offset - Vector3(" + kEarSize + L", 0, 0)", body, shape);
    bind(c, visual, L"CenterPoint", L"pill.CenterPoint + Vector3(" + kEarSize + L", 0, 0)", body, shape);
    bind(c, visual, L"Scale", L"pill.Scale", body, shape);
}

Color clear(Color c)
{
    c.a = 0;
    return c;
}

} // namespace

void Blob::buildShadow(Compositor const& c, ContainerVisual const& parent)
{
    // CreateVisualSurface is Windows 10 1903+; on 1809 the island just has no
    // shadow (decision 015: newer APIs only behind runtime checks).
    using winrt::Windows::Foundation::Metadata::ApiInformation;
    if (!ApiInformation::IsMethodPresent(L"Windows.UI.Composition.Compositor", L"CreateVisualSurface")) return;

    // The snapshot is of an outline-only twin of the body: the same background
    // and ears, no content. Snapshotting the body itself re-rendered and
    // re-blurred the shadow on every frame of anything animating inside it
    // (the equalizer, a swimming title, the progress bar): all the time music
    // played, which was enough GPU load to make audio stutter. The twin only
    // changes when the shape does (morph, press, drag). It sits right under
    // the body with the same colours, always opaque (solid_): solid, it's
    // never seen; as glass, the glass covers it (decision 089).
    outline_ = c.CreateShapeVisual();
    bind(c, outline_, L"Size", L"pill.Size", body_, shape_);
    bind(c, outline_, L"Offset", L"pill.Offset", body_, shape_);
    bind(c, outline_, L"CenterPoint", L"pill.CenterPoint", body_, shape_);
    bind(c, outline_, L"Scale", L"pill.Scale", body_, shape_);
    addBackground(c, outline_, shape_, solid_.gradient);
    addEars(c, outline_, shape_, solid_.leftEar, solid_.rightEar);

    snapshot_ = c.CreateVisualSurface(); // live: re-rendered as the outline changes, on the compositor
    snapshot_.SourceVisual(outline_);
    // As wide as the body plus its ears (decision 025), so the shadow follows their curve.
    bind(c, snapshot_, L"SourceOffset", L"Vector2(-" + kEarSize + L", 0)", body_, shape_);
    bind(c, snapshot_, L"SourceSize", L"Vector2(pill.Size.X + 2 * " + kEarSize + L", pill.Size.Y)", body_, shape_);
    shadow_ = c.CreateDropShadow();
    shadow_.Mask(c.CreateSurfaceBrush(snapshot_)); // the body's alpha: its rounded shape, not its rectangle
    applyShadow();

    caster_ = c.CreateSpriteVisual(); // no brush: only its shadow shows
    coverBodyAndEars(c, caster_, body_, shape_);
    caster_.Shadow(shadow_);
    parent.Children().InsertAtTop(caster_);
    parent.Children().InsertAtTop(outline_); // above the shadow, below the body (inserted next)
}

void Blob::applyShadow()
{
    if (!shadow_) return;
    const Tokens& t = tokens();
    shadow_.Color(toUiColor(t.glass ? t.glassShadowColor : t.shadowColor));
    shadow_.BlurRadius(t.shadowBlur);
    shadow_.Offset(toFloat3(t.shadowOffset));
}

void Blob::buildRim(Compositor const& c)
{
    // The background's outline (blob_shape.cpp) inset by half the rim's
    // width: a stroke is centred on its outline, so it's all inside the edge,
    // over the glass, never past it. A radius taller and shifted up like the
    // background, so with the island on the screen edge its top is clipped
    // away. RimInset is set by applyGlass().
    const std::wstring radius = L"Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius)";
    auto outline = c.CreateRoundedRectangleGeometry();
    bind(c, outline, L"CornerRadius", L"Vector2(Max(" + radius + L" - shape.RimInset, 0), Max(" + radius + L" - shape.RimInset, 0))",
         body_, shape_);
    bind(c, outline, L"Offset", L"Vector2(shape.RimInset, shape.RimInset - shape.Attached * " + radius + L")", body_, shape_);
    bind(c, outline, L"Size",
         L"Vector2(Max(pill.Size.X - 2 * shape.RimInset, 0), Max(pill.Size.Y + shape.Attached * " + radius + L" - 2 * shape.RimInset, 0))",
         body_, shape_);
    // Brightest along the bottom, fading out up the sides before the top: the
    // island hangs from the screen edge, so its bottom is the edge in the
    // light, and the rim never meets the ears (which have none). Relative to
    // the outline's bounds, top to bottom. See-through until glass is on.
    Color rim = clear(tokens().glassRimColor);
    rimTop_ = c.CreateColorGradientStop(0.4f, toUiColor(rim));
    rimBottom_ = c.CreateColorGradientStop(1.0f, toUiColor(rim));
    auto brush = c.CreateLinearGradientBrush();
    brush.StartPoint({ 0.5f, 0.0f });
    brush.EndPoint({ 0.5f, 1.0f });
    brush.ColorStops().Append(rimTop_);
    brush.ColorStops().Append(rimBottom_);
    rim_ = c.CreateSpriteShape(outline);
    rim_.StrokeBrush(brush);
    body_.Shapes().Append(rim_);
}

void Blob::buildGlass()
{
    glassTried_ = true;
    if (!snapshot_) return; // no snapshot before 1903; glass is Windows 11's anyway
    luminosity_ = compositor_.CreateColorBrush(toUiColor(tokens().glassLuminosityColor)); // applyGlass() fades it
    auto brush = maskedBackdrop(compositor_, compositor_.CreateSurfaceBrush(snapshot_), luminosity_);
    if (!brush) return; // logged: the island stays solid
    glass_ = compositor_.CreateSpriteVisual();
    glass_.Brush(brush);
    glass_.Opacity(0.0f);
    coverBodyAndEars(compositor_, glass_, body_, shape_);
    outline_.Parent().Children().InsertAbove(glass_, outline_); // over the solid twin, under the body
}

void Blob::applyGlass(Ms duration)
{
    const Tokens& t = tokens();
    // Ears overlap the body only while it's opaque (blob_shape.cpp).
    shape_.InsertScalar(L"Overlap", t.glass ? 0.0f : 1.0f);
    shape_.InsertScalar(L"RimInset", t.glassRimWidth / 2);
    rim_.StrokeThickness(t.glassRimWidth);
    fadeColor(rimTop_, clear(t.glassRimColor), duration);
    fadeColor(rimBottom_, t.glass ? t.glassRimColor : clear(t.glassRimColor), duration);

    if (t.glass && !glass_ && !glassTried_) buildGlass();
    if (luminosity_) fadeColor(luminosity_, t.glassLuminosityColor, duration);
    if (!glass_ || t.glass == glassShown_) return;
    glassShown_ = t.glass;
    // On, it's there at once: the body over it still has its old, opaque
    // colours, and fades to see-through. Off, it fades out as the body fades
    // back in, uncovering the solid twin under it in the same colours.
    fadeTo(compositor_, glass_, t.glass ? 1.0f : 0.0f, t.glass ? Ms{ 1 } : duration);
}

} // namespace cup::platform
