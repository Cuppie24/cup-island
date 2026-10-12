#pragma once
// Pieces of a Blob's outline that follow its live, animating size through
// expressions, so every morph, press squash and drag carries them with no
// app code per frame.

#include "core/tokens.h"
#include "platform/win.h"

#include <string>

namespace cup::platform {

// A tokens.json curve as a Composition easing.
wuc::CubicBezierEasingFunction easingFor(wuc::Compositor const& c, Bezier curve);

// Binds target.property to an expression that can read the body visual as
// `pill` and the Blob's property set as `shape` (MaxRadius, FlareRadius,
// Attached, Flare, LeftEar, RightEar).
void bindToBlob(wuc::Compositor const& c, wuc::CompositionObject const& target, std::wstring const& property,
                std::wstring const& expression, wuc::ShapeVisual const& body, wuc::CompositionPropertySet const& shape);

// The flared top corners' size E, as an expression (decision 025): the
// flareRadius token, no taller than the shape (none when Hidden), times
// `Flare` (0 off the screen edge: a dragged bubble, a lifted island). Each
// side also has its own factor, 0 where that side is flush with the screen's
// edge (decision 026); this one, without it, is the larger of the two.
inline const std::wstring kEarSize = L"(shape.Flare * Min(shape.FlareRadius, pill.Size.Y))";
inline std::wstring earSize(bool left) { return L"(" + kEarSize + (left ? L" * shape.LeftEar)" : L" * shape.RightEar)"); }

// Adds the two flared top corners ("ears") beside the body's top edge, as
// children of the body, filled with `fill` (decision 025).
void addEars(wuc::Compositor const& c, wuc::ShapeVisual const& body, wuc::CompositionPropertySet const& shape,
             wuc::CompositionBrush const& fill);

} // namespace cup::platform
