// A Blob's lens (decision 165): what makes its glass liquid. Along the sides
// and the bottom, in a band inside the edge, the blurred desktop shows a
// second time over the glass: as bright as it is, and pulled in from the
// edge, the way a thick pane bends what's behind it. All of it runs on the
// compositor: no screen capture, no app code per frame (TR §5).

#include "platform/blob.h"

#include "platform/blob_shape.h"
#include "platform/element_parts.h"
#include "platform/glass.h"
#include "platform/log.h"

#include <cstdint>
#include <string>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// The band fades inward in steps: strokes from the edge, each wider than the
// last and drawn over it, so the edge has all of them and the inside one.
// Their opacities add up to 1 at the edge, 0.62 in the middle, 0.3 inside.
struct Step {
    float reach;   // of LensWidth
    float opacity; // this stroke's own
};
constexpr Step kSteps[] = { { 1.0f / 3, 1.0f }, { 2.0f / 3, 0.45f }, { 1.0f, 0.3f } };

Color white(float opacity) { return { 255, 255, 255, static_cast<std::uint8_t>(opacity * 255 + 0.5f) }; }

} // namespace

void Blob::buildLens()
{
    lensTried_ = true;
    if (!glass_) return; // only over the glass: Windows 11 (decision 089)
    Compositor const& c = compositor_;

    // The band: the lens's mask. Like the rim, clear up the sides until 40%
    // of the way down, so it never meets the ears or the screen edge.
    band_ = c.CreateShapeVisual();
    band_.BorderMode(CompositionBorderMode::Soft); // a snapshot's root: antialiased only if told (decision 133)
    bindToBlob(c, band_, L"Size", L"pill.Size", body_, shape_);
    for (const Step& step : kSteps) {
        const std::wstring width = L"(shape.LensWidth * " + std::to_wstring(step.reach) + L")";
        auto brush = c.CreateLinearGradientBrush();
        brush.StartPoint({ 0.5f, 0.0f });
        brush.EndPoint({ 0.5f, 1.0f });
        brush.ColorStops().Append(c.CreateColorGradientStop(0.4f, toUiColor(white(0))));
        brush.ColorStops().Append(c.CreateColorGradientStop(1.0f, toUiColor(white(step.opacity))));
        auto stroke = c.CreateSpriteShape(insetOutline(c, body_, shape_, width + L" / 2")); // centred: all inside the edge
        stroke.StrokeBrush(brush);
        bindToBlob(c, stroke, L"StrokeThickness", width, body_, shape_);
        band_.Shapes().Append(stroke);
    }
    bandSnapshot_ = c.CreateVisualSurface();
    bandSnapshot_.SourceVisual(band_);
    // As wide as the outline's snapshot (blob_backdrop): the body and its ears.
    bindToBlob(c, bandSnapshot_, L"SourceOffset", L"Vector2(-" + kEarSize + L", 0)", body_, shape_);
    bindToBlob(c, bandSnapshot_, L"SourceSize", L"Vector2(pill.Size.X + 2 * " + kEarSize + L", pill.Size.Y)", body_, shape_);

    Lens made = lensBackdrop(c, c.CreateSurfaceBrush(bandSnapshot_));
    if (!made.brush) return; // logged: glass without a lens
    if (made.bend) {
        // What shows is the desktop magnified about the middle of the top
        // edge, by just enough that each side and the bottom show what's
        // LensDepth px further in: (1 + 2 depth / width) across, (1 + depth /
        // height) down. The pivot's x is the body's middle in the lens's
        // space, which starts an ear to its left. Bound to the live size, so
        // the bend keeps its depth through every morph.
        const std::wstring across = L"(2 * shape.LensDepth / Max(pill.Size.X, 1))";
        const std::wstring down = L"(shape.LensDepth / Max(pill.Size.Y, 1))";
        try {
            bindToBlob(c, made.bend.Properties(), L"Lens.TransformMatrix",
                       L"Matrix3x2(1 + " + across + L", 0, 0, 1 + " + down + L", -" + across + L" * (" + kEarSize + L" + pill.Size.X / 2), 0)",
                       body_, shape_);
        } catch (winrt::hresult_error const& e) {
            // Left at the identity: the lip, without the bending.
            logLine("glass: the lens's bend can't follow the island (0x%08lx)", static_cast<unsigned long>(e.code()));
        }
    }
    lens_ = c.CreateSpriteVisual();
    lens_.Brush(made.brush);
    lens_.Opacity(0.0f);
    coverBodyAndEars(c, lens_, body_, shape_);
    glass_.Parent().Children().InsertAbove(lens_, glass_); // over the glass, under the twin's tint
}

void Blob::applyLens(Ms duration)
{
    const Tokens& t = tokens();
    shape_.InsertScalar(L"LensWidth", t.glassLensWidth);
    shape_.InsertScalar(L"LensDepth", t.glassLensDepth);
    if (glassShown_ && !lensTried_ && glassLensShare(t) > 0) buildLens();
    // With the glass: under the twin's layer, which is what fades (decision 097).
    if (lens_) fadeTo(compositor_, lens_, glassShown_ ? glassLensShare(t) : 0.0f, duration);
}

} // namespace cup::platform
