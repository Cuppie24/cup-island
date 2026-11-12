// What a Blob draws besides its body: the outline twin under it, the shadow
// it casts (decision 018), and as glass, the blurred desktop between the
// two and a rim on the body (decision 089).

#include "platform/blob.h"

#include "platform/blob_shape.h"
#include "platform/element_parts.h"
#include "platform/glass.h"
#include "platform/log.h"

#include <winrt/Windows.Foundation.Metadata.h>

#include <algorithm>
#include <cmath>
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
    // never seen; as glass, its layer, see-through, is the tint over the
    // glass (decision 097).
    outline_ = c.CreateShapeVisual();
    bind(c, outline_, L"Size", L"pill.Size", body_, shape_);
    bind(c, outline_, L"Offset", L"pill.Offset", body_, shape_);
    // Not its Scale: that's its layer's, below (decision 097).
    addBackground(c, outline_, shape_, solid_.gradient);
    // Always overlapping the body, even as glass: the twin is the glass's
    // mask, and a seam in it let the desktop through between the island and
    // its corners whenever it sat between pixels, mid-motion (decision 097).
    addEars(c, outline_, shape_, solid_.leftEar, solid_.rightEar, L"1");

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
    // A layer, made see-through as one: as glass it's the island's tint
    // (decision 097). From the parent's origin past the body's far corner
    // (its right ear included), where the twin can be.
    twin_ = c.CreateLayerVisual();
    bind(c, twin_, L"Size", L"Vector2(pill.Offset.X + pill.Size.X + " + kEarSize + L" + 2, pill.Offset.Y + pill.Size.Y + 2)", body_, shape_);
    twin_.Children().InsertAtTop(outline_);
    // The press's shrink (and any scale) is the layer's, about the body's
    // centre point in the parent's space: the twin on screen shrinks with the
    // body, but its snapshot, drawn without its parents, stays full size. The
    // glass and the shadow scale it with themselves (coverBodyAndEars);
    // scaled in the snapshot too, the glass's cut shrank twice while the pill
    // was held, and a gap opened at its corners (decision 097).
    bind(c, twin_, L"CenterPoint", L"pill.Offset + pill.CenterPoint", body_, shape_);
    bind(c, twin_, L"Scale", L"pill.Scale", body_, shape_);
    parent.Children().InsertAtTop(twin_); // above the shadow, below the body (inserted next)
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

void Blob::buildGlow(Compositor const& c, CompositionGeometry const& outline)
{
    // A vertical gradient on the background's own outline, so it has the
    // island's shape. Stops are a share of that outline's height (which
    // reaches a radius above the body while it hangs from the screen edge),
    // so they follow it: the glow reaches glassGlowHeight px up, whatever
    // the card's height.
    // Many stops sampling glowFade's curve (kGlowStops), not a straight
    // ramp: the colour hugs the edge and melts away with no line.
    Color none = clear(tokens().islandGlowColor);
    const std::wstring height = L"Max(pill.Size.Y + shape.Attached * Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius), 1)";
    bool bound = true;
    glow_.clear();
    for (int i = 0; i < kGlowStops; ++i) {
        float up = i / float(kGlowStops - 1);
        // Until bound (or if it can't be): a fixed share of the outline's height.
        auto stop = c.CreateColorGradientStop(1.0f - 0.15f * up, toUiColor(none));
        if (bound) {
            try {
                bind(c, stop, L"Offset", L"Max(1 - shape.GlowHeight * " + std::to_wstring(i) + L" / " + std::to_wstring(kGlowStops - 1) + L" / " + height + L", 0)", body_, shape_);
            } catch (winrt::hresult_error const& e) {
                bound = false; // taller on a tall card, then
                logLine("glass: the glow can't follow the island's height (0x%08lx)", static_cast<unsigned long>(e.code()));
            }
        }
        glow_.push_back(stop);
    }
    auto brush = c.CreateLinearGradientBrush();
    brush.StartPoint({ 0.5f, 0.0f });
    brush.EndPoint({ 0.5f, 1.0f });
    for (auto it = glow_.rbegin(); it != glow_.rend(); ++it) brush.ColorStops().Append(*it); // top first: rising offsets
    auto glow = c.CreateSpriteShape(outline);
    glow.FillBrush(brush);
    body_.Shapes().Append(glow);
}

void Blob::fadeGlow(Ms duration)
{
    const Tokens& t = tokens();
    const Color& glow = t.islandGlowColor;
    for (std::size_t i = 0; i < glow_.size(); ++i) {
        Color c = glow;
        c.a = static_cast<std::uint8_t>(std::lround(glow.a * glowFade(t, i / float(kGlowStops - 1))));
        fadeColor(glow_[i], c, duration);
    }
}

void Blob::buildGlass()
{
    glassTried_ = true;
    if (!snapshot_) return; // no snapshot before 1903; glass is Windows 11's anyway
    luminosity_ = device_->createSurface({ 8, 8 }); // stretched over the glass: one colour everywhere
    paintLuminosity(tokens().glassLuminosityColor);
    auto luminosity = compositor_.CreateSurfaceBrush(luminosity_);
    luminosity.Stretch(wuc::CompositionStretch::Fill);
    glassLuminosity_ = luminosity;
    glassMask_ = compositor_.CreateSurfaceBrush(snapshot_);
    darkenBuilt_ = { tokens().glassDarkenScale, tokens().glassDarkenLift };
    Glass made = maskedBackdrop(compositor_, glassMask_, glassLuminosity_, darkenBuilt_);
    if (!made.brush) return; // logged: the island stays solid
    glassLook_ = made.look;
    glass_ = compositor_.CreateSpriteVisual();
    glass_.Brush(made.brush);
    glass_.Opacity(0.0f);
    coverBodyAndEars(compositor_, glass_, body_, shape_);
    twin_.Parent().Children().InsertBelow(glass_, twin_); // under the twin's layer, its tint (decision 097)
}

void Blob::paintLuminosity(Color color)
{
    // A lost GPU loses this too; it's painted again with the next new colour.
    device_->draw(luminosity_, [&](ID2D1DeviceContext* dc) {
        dc->Clear(D2D1_COLOR_F{ color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f });
    });
    luminosityPainted_ = color;
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
    if (glassLook_ == Look::Even && t.glassLuminosityColor != luminosityPainted_) paintLuminosity(t.glassLuminosityColor);
    // Darkened's range is baked into its effect: a new one from tokens.json
    // makes the brush again (decision 100).
    if (glass_ && glassLook_ == Look::Darkened && (t.glassDarkenScale != darkenBuilt_.scale || t.glassDarkenLift != darkenBuilt_.lift)) {
        darkenBuilt_ = { t.glassDarkenScale, t.glassDarkenLift };
        Glass made = maskedBackdrop(compositor_, glassMask_, glassLuminosity_, darkenBuilt_);
        if (made.brush && made.look == Look::Darkened) glass_.Brush(made.brush);
    }
    if (glass_ && t.glass != glassShown_) {
        glassShown_ = t.glass;
        // On, it's there at once, under the twin's layer, still opaque, which
        // fades to its see-through tint. Off, it fades out as the layer
        // fades back to opaque over it.
        fadeTo(compositor_, glass_, t.glass ? 1.0f : 0.0f, t.glass ? Ms{ 1 } : duration);
    }
    // As glass, the twin's layer is the island's tint over it (decision
    // 097): seamless, since the layer is see-through as one, and with no
    // opaque fringe along the curves (the reason it was hidden before). A
    // Transparency pick fades it too. Solid, it's opaque under the body.
    if (twin_) fadeTo(compositor_, twin_, glassShown_ ? glassShare(t, glassLook_ != Look::Plain) : 1.0f, duration);
}

} // namespace cup::platform
