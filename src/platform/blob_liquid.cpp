// A Blob's liquid glass (decision 165): the bent picture of the desktop, in
// a sprite of its own over the frosted glass, shown only while a card is
// open (decision 172).

#include "platform/blob.h"

#include "core/liquid_glass.h"
#include "platform/blob_shape.h"
#include "platform/element_parts.h"
#include "platform/glass.h"
#include "platform/log.h"

#include <string>

namespace cup::platform {
namespace {

constexpr auto bind = bindToBlob;

} // namespace

void Blob::setLiquid(wuc::ICompositionSurface const& picture)
{
    if (picture == liquid_) return;
    if (liquidGlass_) { // another picture, or none: the old one goes at once
        liquidGlass_.Parent().Children().Remove(liquidGlass_);
        liquidGlass_ = nullptr;
    }
    liquidShown_ = false;
    liquid_ = picture;
    if (liquid_ && glass_) {
        try {
            liquidGlass_ = compositor_.CreateSpriteVisual();
            liquidGlass_.Brush(liquidBrush());
            liquidGlass_.Opacity(0.0f); // until showLiquid()
            coverBodyAndEars(compositor_, liquidGlass_, body_, shape_);
            glass_.Parent().Children().InsertAbove(liquidGlass_, glass_); // under the twin's tint, like the glass
        } catch (winrt::hresult_error const& e) {
            logGlassError("liquid: a shape can't show it, the frosted glass stays", "sprite", e);
            liquidGlass_ = nullptr;
            liquid_ = nullptr; // not tried again until it's handed over anew
        }
    } else if (liquid_) {
        logLine("glass: liquid: no frosted glass under it on this shape: not shown");
    }
    applyGlass(recolorDuration(tokens()));
    fadeGlow(recolorDuration(tokens())); // the rim's line: brighter while liquid glass is picked (decisions 165, 166)
}

void Blob::showLiquid(bool on)
{
    on = on && liquidGlass_;
    if (on == liquidShown_) return;
    liquidShown_ = on;
    // A quick crossfade with the frosted glass under it: the two differ in
    // blur and brightness, and a cut between them showed.
    Ms duration = tokens().hoverFadeDuration;
    if (liquidGlass_) fadeTo(compositor_, liquidGlass_, on ? 1.0f : 0.0f, duration);
    logLine("glass: a shape shows %s", on ? "liquid glass" : "the frosted glass");
    if (glassLook_ == Look::Plain) applyGlass(duration); // the tint over a plain blur is stronger than over liquid glass
}

wuc::CompositionBrush Blob::liquidBrush()
{
    // The picture is the render layer's size and drawn at its pixels: not
    // stretched, and moved back by where this sprite is (the body's corner,
    // an ear to the left), so each of its pixels stays where the desktop's
    // is through every morph and drag.
    auto picture = compositor_.CreateSurfaceBrush(liquid_);
    picture.Stretch(wuc::CompositionStretch::None);
    picture.HorizontalAlignmentRatio(0.0f);
    picture.VerticalAlignmentRatio(0.0f);
    bind(compositor_, picture, L"Offset", L"Vector2(" + kEarSize + L" - pill.Offset.X, -pill.Offset.Y)", body_, shape_);
    auto mask = compositor_.CreateSurfaceBrush(snapshot_); // the outline: exact, the compositor's (decision 018)
    mask.Stretch(wuc::CompositionStretch::Fill);
    auto cut = compositor_.CreateMaskBrush();
    cut.Source(picture);
    cut.Mask(mask);
    return cut;
}

} // namespace cup::platform
