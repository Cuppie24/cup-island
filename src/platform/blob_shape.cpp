#include "platform/blob_shape.h"

namespace cup::platform {

using namespace winrt::Windows::UI::Composition;

CubicBezierEasingFunction easingFor(Compositor const& c, Bezier curve)
{
    return c.CreateCubicBezierEasingFunction(toFloat2(curve.c1), toFloat2(curve.c2));
}

void bindToBlob(Compositor const& c, CompositionObject const& target, std::wstring const& property,
                std::wstring const& expression, ShapeVisual const& body, CompositionPropertySet const& shape)
{
    auto anim = c.CreateExpressionAnimation(expression);
    anim.SetReferenceParameter(L"pill", body);
    anim.SetReferenceParameter(L"shape", shape);
    target.StartAnimation(property, anim);
}

// Background (blob_shape.h): only the bottom corners are rounded (a notch hanging from the
// screen edge). The geometry has one radius for all corners, so it's made a
// radius taller and shifted up; ShapeVisual clips the top corners away.
// Radius = half the height, capped by maxCornerRadius: the same curve as core
// pillCornerRadius(), which content radii are nested in. Dragged off the edge
// (Attached 0) the top corners round too: the square bubble becomes a circle
// (decision 025). Built on `target`, whose Size is the body's.
void addBackground(Compositor const& c, ShapeVisual const& target, CompositionPropertySet const& shape, CompositionBrush const& fill)
{
    auto background = c.CreateRoundedRectangleGeometry();
    bindToBlob(c, background, L"CornerRadius",
        L"Vector2(Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius), Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))",
        target, shape);
    bindToBlob(c, background, L"Offset", L"Vector2(0, -shape.Attached * Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", target, shape);
    bindToBlob(c, background, L"Size",
        L"Vector2(pill.Size.X, pill.Size.Y + shape.Attached * Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", target, shape);
    auto backgroundShape = c.CreateSpriteShape(background);
    backgroundShape.FillBrush(fill);
    target.Shapes().Append(backgroundShape);
}

void addEars(Compositor const& c, ShapeVisual const& body, CompositionPropertySet const& shape, CompositionBrush const& leftFill,
             CompositionBrush const& rightFill)
{
    // Each ear is an E×E square beside the top edge, minus a quarter circle of
    // radius E centred on its bottom outer corner: the edge curving down into
    // the body's side. Composition has no concave corners, and a path needs
    // Direct2D interop, so it's built from plain shapes instead: a ring
    // around that corner whose inner edge is the curve, thick enough (E) to
    // cover the whole square (its far corner is 1.41 E away), clipped to the
    // square by its ShapeVisual. Sized in pixels, not scaled, so the curve is
    // tessellated at the size it's shown. Each ear reaches shape.Overlap px
    // into the body: 1 while the island's colours are opaque, so no seam shows
    // where two antialiased edges would meet at a fractional size; 0 as glass
    // (decision 089), where a see-through overlap would draw a darker line.
    for (bool left : { true, false }) {
        const std::wstring e = earSize(left);
        auto ring = c.CreateEllipseGeometry();
        bindToBlob(c, ring, L"Center", left ? L"Vector2(0, " + e + L")" : L"Vector2(Min(" + e + L", shape.Overlap) + " + e + L", " + e + L")", body, shape);
        bindToBlob(c, ring, L"Radius", L"Vector2(1.5 * " + e + L", 1.5 * " + e + L")", body, shape); // the stroke's middle
        auto stroke = c.CreateSpriteShape(ring);
        stroke.StrokeBrush(left ? leftFill : rightFill);
        bindToBlob(c, stroke, L"StrokeThickness", e, body, shape);

        auto ear = c.CreateShapeVisual(); // clips the ring to the ear's square
        ear.Shapes().Append(stroke);
        // Min(E, Overlap): no sliver of an ear when there's no flare.
        bindToBlob(c, ear, L"Size", L"Vector2(" + e + L" + Min(" + e + L", shape.Overlap), " + e + L")", body, shape);
        bindToBlob(c, ear, L"Offset", left ? L"Vector3(-" + e + L", 0, 0)" : L"Vector3(pill.Size.X - Min(" + e + L", shape.Overlap), 0, 0)",
                   body, shape);
        body.Children().InsertAtBottom(ear);
    }
}

} // namespace cup::platform
