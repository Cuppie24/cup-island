#pragma once
// Composition building blocks for single content items: how a progress bar,
// waveform, button or swimming text is put together and animated.
// ElementVisuals decides which to build and when; these only build.

#include "core/content.h"
#include "core/tokens.h"
#include "platform/progress_wave.h"
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
// Moved (a row opened above it pushes it down, decision 132): from where it was to `to`, eased,
// both ends given (decision 008). Its Offset is `to` from the start: nothing else waits on it.
// `duration`: 0 = scrollDuration, as a list's rows glide.
void glideFrom(wuc::Compositor const& c, wuc::Visual const& visual, wfn::float3 from, wfn::float3 to, Ms duration = Ms{ 0 });
// A pill growing out of its start (GroupFill::unrolls, decision 180): `in`, from a square of its
// height at its left end to its whole `size`; out, back to it, fading, then `done`. Its CenterPoint its middle.
void unroll(wuc::Compositor const& c, wuc::Visual const& visual, Vec2 size, wfn::float3 at, bool in, std::function<void()> done = {});
// Built at its new size `to` and place `at`, it grows or shrinks there from the size `from` and the
// place `was` it had, its top left corner leading (a card's cover, decision 180). Its CenterPoint
// must be its middle, as every element's is.
void resizeFrom(wuc::Compositor const& c, wuc::Visual const& visual, Vec2 from, Vec2 to, wfn::float3 was, wfn::float3 at, Ms duration);
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
    WaveParts wave; // a wavy bar's (decision 167): the fill is then unseen, and only sets where the wave ends
    bool pinned = false; // ...and its knob is the upright one, always there
};
float progressOverhang();
// `wavy` (Progress::wavy): `factory` makes the wave's path; if it can't be made, a plain bar.
// `pinned`: wavy and seekable, so its knob is always shown.
ProgressParts createProgress(wuc::Compositor const& c, Vec2 frame, const Color& accent, bool wavy = false, bool pinned = false,
                             ID2D1Factory* factory = nullptr);
// Fill grows from where playback is now to full width, entirely on the compositor.
void armProgress(wuc::Compositor const& c, wuc::CompositionRoundedRectangleGeometry const& fill, const Timeline& timeline,
                 float width, Ms now);
void holdProgress(wuc::CompositionRoundedRectangleGeometry const& fill, float fillWidth); // scrubbing: stop and pin
// `visible`: under the pointer or dragged. `pinned` (ProgressParts::pinned): always there, bigger then.
void showKnob(wuc::Compositor const& c, wuc::CompositionSpriteShape const& knob, bool visible, bool pinned = false);

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
