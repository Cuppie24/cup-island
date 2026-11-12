#pragma once
// Composition building blocks for single content items: how a progress bar,
// waveform, button or swimming text is put together and animated.
// ElementVisuals decides which to build and when; these only build.

#include "core/content.h"
#include "core/tokens.h"
#include "platform/win.h"

#include <vector>

namespace cup::platform {

// Content appearing: fades in to `opacity` (a muted tab's content:
// tabMutedOpacity, decision 072) after `delay` (the morph's reveal, plus its
// row's stagger: decision 080). `arrive`: new content, not a rebuild in place:
// it also grows from contentArriveScale and settles.
void fadeIn(wuc::Compositor const& c, wuc::Visual const& visual, float opacity, Ms delay, bool arrive);
void fadeTo(wuc::Compositor const& c, wuc::Visual const& visual, float opacity, Ms duration);
// A new picture (a cover read after its track showed, decision 056): `host`
// shows `fresh` at once, under a copy of the old picture that fades out.
void fadeInSurface(wuc::SpriteVisual const& host, wuc::CompositionDrawingSurface const& fresh, Ms duration);
void playScale(wuc::Compositor const& c, wuc::Visual const& visual, float scale, Ms duration, Bezier curve);
// A colour (a brush's, a shadow's) to a new one (a cover, a theme; decision 051): over recolorDuration() (the picked speed, decision 053).
void fadeColor(wuc::CompositionObject const& target, const Color& color);
void fadeColor(wuc::CompositionObject const& target, const Color& color, Ms duration); // a quick answer to a click (a switch)

// Progress: track + fill + a knob riding the fill's end (hidden until hover).
// The visual is wider than the frame by the knob's radius on each side, so the
// knob isn't clipped at 0% or 100%.
struct ProgressParts {
    wuc::ShapeVisual visual{ nullptr };
    wuc::CompositionRoundedRectangleGeometry fill{ nullptr };
    wuc::CompositionSpriteShape knob{ nullptr };
    wuc::CompositionColorBrush track{ nullptr }, accent{ nullptr }, knobBrush{ nullptr }; // faded on a recolour
};
float progressOverhang();
ProgressParts createProgress(wuc::Compositor const& c, Vec2 frame, const Color& accent);
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
    wuc::CompositionColorBrush track{ nullptr }, accent{ nullptr };
};
RingParts createRing(wuc::Compositor const& c, Vec2 frame, const Color& accent);
void armRing(wuc::Compositor const& c, wuc::CompositionEllipseGeometry const& arc, const Timeline& timeline, Ms now);

// Waveform: rounded bars that rise and fall while playing and rest as dots when paused.
struct WaveformParts {
    wuc::ShapeVisual visual{ nullptr };
    std::vector<wuc::CompositionRoundedRectangleGeometry> bars;
    wuc::CompositionColorBrush accent{ nullptr }; // shared by every bar
};
WaveformParts createWaveform(wuc::Compositor const& c, Vec2 frame, const Color& accent);
void animateWaveform(wuc::Compositor const& c, const WaveformParts& parts, float height, bool active);

// Button: a hover shape (nested radius, hidden) under the glyph surface.
struct ButtonParts {
    wuc::ContainerVisual visual{ nullptr };
    wuc::ShapeVisual highlight{ nullptr };
    wuc::CompositionColorBrush hover{ nullptr };
    wuc::SpriteVisual glyph{ nullptr }; // shows the painted surface: crossfaded on a recolour
};
// `hover`: the shape's colour (buttonHoverColor; a text field uses inputColor).
ButtonParts createButton(wuc::Compositor const& c, Vec2 frame, float cornerRadius, wuc::CompositionDrawingSurface const& glyph,
                         const Color& hover);

// Swimming text: shows `surface` (see paintMarquee) through soft-edged mask,
// pausing at the start, then scrolling `travel` pixels left, forever.
// `ink`: the text is painted white and takes this brush's colour, which
// fades (decision 102): one effect over the edges, the text and the colour,
// since Windows takes no mask in a mask. `inked` false: Windows refused it,
// the text keeps its own colour (logged once).
struct MarqueeBrush {
    wuc::CompositionBrush brush{ nullptr };
    bool inked = false;
};
MarqueeBrush createMarqueeBrush(wuc::Compositor const& c, wuc::CompositionDrawingSurface const& surface, Vec2 visualSize,
                                float fadeWidth, float travel, wuc::CompositionColorBrush const& ink = nullptr);

} // namespace cup::platform
