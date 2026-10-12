#pragma once
// Pieces of a Blob's outline that follow its live, animating size through
// expressions, so every morph, press squash and drag carries them with no
// app code per frame.

#include "platform/win.h"

#include <string>

namespace cup::platform {

// Binds target.property to an expression that can read the body visual as
// `pill` and the Blob's property set as `shape` (MaxRadius, FlareRadius, Ears).
void bindToBlob(wuc::Compositor const& c, wuc::CompositionObject const& target, std::wstring const& property,
                std::wstring const& expression, wuc::ShapeVisual const& body, wuc::CompositionPropertySet const& shape);

// The flared top corners' size E, as an expression (decision 025): the
// flareRadius token, no taller than the shape (none when Hidden), times
// `Ears` (0 while the bubble is dragged away from the edge).
inline const std::wstring kEarSize = L"(shape.Ears * Min(shape.FlareRadius, pill.Size.Y))";

// Adds the two flared top corners ("ears") beside the body's top edge, as
// children of the body, filled with `fill` (decision 025).
void addEars(wuc::Compositor const& c, wuc::ShapeVisual const& body, wuc::CompositionPropertySet const& shape,
             wuc::CompositionBrush const& fill);

} // namespace cup::platform
