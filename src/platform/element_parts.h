#pragma once
// Composition building blocks for single content items: how a progress bar,
// waveform, button or swimming text is put together and animated.
// ElementVisuals decides which to build and when; these only build.

#include "core/content.h"
#include "core/tokens.h"
#include "platform/win.h"

#include <vector>

namespace cup::platform {

// Content appearing after a mode change: fades in once the pill starts morphing.
void fadeIn(wuc::Compositor const& c, wuc::Visual const& visual);
void fadeTo(wuc::Compositor const& c, wuc::Visual const& visual, float opacity, Ms duration);
void playScale(wuc::Compositor const& c, wuc::Visual const& visual, float scale, Ms duration, Bezier curve);

// Progress: track + fill + a knob riding the fill's end (hidden until hover).
// The visual is wider than the frame by the knob's radius on each side, so the
// knob isn't clipped at 0% or 100%.
struct ProgressParts {
    wuc::ShapeVisual visual{ nullptr };
    wuc::CompositionRoundedRectangleGeometry fill{ nullptr };
    wuc::CompositionSpriteShape knob{ nullptr };
};
float progressOverhang();
ProgressParts createProgress(wuc::Compositor const& c, Vec2 frame, const cup::Color& accent);
// Fill grows from where playback is now to full width, entirely on the compositor.
void armProgress(wuc::Compositor const& c, wuc::CompositionRoundedRectangleGeometry const& fill, const Timeline& timeline,
                 float width, Ms now);
void holdProgress(wuc::CompositionRoundedRectangleGeometry const& fill, float fillWidth); // scrubbing: stop and pin
void showKnob(wuc::Compositor const& c, wuc::CompositionSpriteShape const& knob, bool visible);

// Ring (decision 010): a faint full circle under an accent arc that starts
// at the top and runs clockwise, filling as the timeline runs. The arc is
// trimmed (TrimEnd), so it animates on the compositor like the bar.
struct RingParts {
    wuc::ShapeVisual visual{ nullptr };
    wuc::CompositionEllipseGeometry arc{ nullptr };
};
RingParts createRing(wuc::Compositor const& c, Vec2 frame, const cup::Color& accent);
void armRing(wuc::Compositor const& c, wuc::CompositionEllipseGeometry const& arc, const Timeline& timeline, Ms now);

// Waveform: rounded bars that rise and fall while playing and rest as dots when paused.
struct WaveformParts {
    wuc::ShapeVisual visual{ nullptr };
    std::vector<wuc::CompositionRoundedRectangleGeometry> bars;
};
WaveformParts createWaveform(wuc::Compositor const& c, Vec2 frame, const cup::Color& accent);
void animateWaveform(wuc::Compositor const& c, const WaveformParts& parts, float height, bool active);

// Button: a hover shape (nested radius, hidden) under the glyph surface.
struct ButtonParts {
    wuc::ContainerVisual visual{ nullptr };
    wuc::ShapeVisual highlight{ nullptr };
};
ButtonParts createButton(wuc::Compositor const& c, Vec2 frame, float cornerRadius, wuc::CompositionDrawingSurface const& glyph);

// Swimming text: shows `surface` (see paintMarquee) through soft-edged mask,
// pausing at the start, then scrolling `travel` pixels left, forever.
wuc::CompositionBrush createMarqueeBrush(wuc::Compositor const& c, wuc::CompositionDrawingSurface const& surface,
                                         Vec2 visualSize, float fadeWidth, float travel);

} // namespace cup::platform
