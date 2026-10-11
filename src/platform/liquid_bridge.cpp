#include "platform/liquid_bridge.h"

#include "core/tokens.h"

#include <algorithm>
#include <string>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Expression building blocks over the two bodies `a` and `b` (live values).
const std::wstring kAx = L"(a.Offset.X + a.Size.X / 2)";
const std::wstring kAy = L"(a.Offset.Y + a.Size.Y / 2)";
const std::wstring kBx = L"(b.Offset.X + b.Size.X / 2)";
const std::wstring kBy = L"(b.Offset.Y + b.Size.Y / 2)";
const std::wstring kDx = L"(" + kBx + L" - " + kAx + L")";
const std::wstring kDy = L"(" + kBy + L" - " + kAy + L")";
const std::wstring kLength = L"Length(Vector2(" + kDx + L", " + kDy + L"))";

// Edge gap, treating both shapes as boxes: negative once they overlap.
const std::wstring kGap = L"Max(Abs(" + kDx + L") - (a.Size.X + b.Size.X) / 2, Abs(" + kDy + L") - (a.Size.Y + b.Size.Y) / 2)";

// Never thicker than the smaller shape (so it can't stick out of it, and is
// zero while the bubble is tucked away at size 0).
const std::wstring kThickness = L"(Min(p.Thickness, Min(a.Size.Y, b.Size.Y) * 0.7) * Clamp(1 - " + kGap + L" / p.Reach, 0, 1))";

// The strand is centred on the midpoint, so a 180° ambiguity doesn't matter:
// Atan (not Atan2) is enough. dx is kept off zero to avoid dividing by it.
const std::wstring kSafeDx = L"(" + kDx + L" >= 0 ? Max(" + kDx + L", 0.01) : Min(" + kDx + L", -0.01))";
const std::wstring kAngle = L"Atan(" + kDy + L" / " + kSafeDx + L")";

void bind(Compositor const& c, CompositionObject const& target, wchar_t const* property, const std::wstring& expression,
          const Blob& a, const Blob& b, CompositionPropertySet const& params)
{
    auto anim = c.CreateExpressionAnimation(expression);
    anim.SetReferenceParameter(L"a", a.body());
    anim.SetReferenceParameter(L"b", b.body());
    anim.SetReferenceParameter(L"p", params);
    target.StartAnimation(property, anim);
}

} // namespace

void LiquidBridge::build(wuc::Compositor const& c, wuc::ContainerVisual const& parent, Vec2 canvas, const Blob& a, const Blob& b)
{
    params_ = c.CreatePropertySet();
    refreshAnimations();

    auto visual = c.CreateShapeVisual();
    visual.Size(toFloat2(canvas));
    auto geometry = c.CreateRoundedRectangleGeometry();
    auto strand = c.CreateSpriteShape(geometry);
    strand.FillBrush(c.CreateColorBrush({ 255, 0, 0, 0 }));
    visual.Shapes().Append(strand);
    parent.Children().InsertAtBottom(visual);

    bind(c, geometry, L"Size", L"Vector2(" + kLength + L", " + kThickness + L")", a, b, params_);
    bind(c, geometry, L"CornerRadius", L"Vector2(" + kThickness + L" / 2, " + kThickness + L" / 2)", a, b, params_);
    bind(c, strand, L"CenterPoint", L"Vector2(" + kLength + L" / 2, " + kThickness + L" / 2)", a, b, params_);
    bind(c, strand, L"Offset",
         L"Vector2((" + kAx + L" + " + kBx + L") / 2 - " + kLength + L" / 2, (" + kAy + L" + " + kBy + L") / 2 - " + kThickness + L" / 2)",
         a, b, params_);
    bind(c, strand, L"RotationAngle", kAngle, a, b, params_);
}

void LiquidBridge::refreshAnimations()
{
    params_.InsertScalar(L"Thickness", tokens().bridgeThickness);
    params_.InsertScalar(L"Reach", std::max(tokens().bridgeReach, 0.01f)); // divided by
}

} // namespace cup::platform
