#include "platform/backdrop_layer.h"

#include "core/tokens.h"
#include "platform/blob_shape.h"
#include "platform/element_painting.h"
#include "platform/element_parts.h"
#include "platform/log.h"

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Dark themes show it stronger than light ones (coverBackdropOpacity: [dark, light]),
// told apart by the island's colour, the way Settings files themes.
float strength()
{
    const Tokens& t = tokens();
    Color island = t.islandColor;
    bool light = 0.2126f * island.r + 0.7152f * island.g + 0.0722f * island.b > 127.5f;
    return light ? t.coverBackdropOpacity.y : t.coverBackdropOpacity.x;
}

} // namespace

void BackdropLayer::build(Compositor const& c, GraphicsDevice& device, ShapeVisual const& body, CompositionPropertySet const& shape,
                          Visual const& content)
{
    compositor_ = c;
    device_ = &device;
    body_ = body;
    layer_ = c.CreateContainerVisual();
    bindToBlob(c, layer_, L"Size", L"pill.Size", body, shape);
    layer_.Clip(c.CreateGeometricClip(islandGeometry(c, body, shape))); // the island's rounded shape, not its box
    layer_.Opacity(strength());
    body.Children().InsertBelow(layer_, content);
}

void BackdropLayer::show(const std::shared_ptr<const ImageData>& image)
{
    if (!layer_ || image == shown_) return;
    shown_ = image;
    Ms fade = recolorDuration(tokens());

    // The old picture fades out and goes; the new one fades in over it.
    if (current_) {
        auto old = current_;
        auto batch = compositor_.CreateScopedBatch(CompositionBatchTypes::Animation);
        fadeTo(compositor_, old, 0.0f, fade);
        batch.End();
        batch.Completed([layer = layer_, old](auto&&, auto&&) { layer.Children().Remove(old); });
        current_ = nullptr;
    }
    if (!image || image->width <= 0 || image->height <= 0) return;

    // Drawn at its own small size; the brush stretches it over the island, which
    // a picture this blurred takes without looking soft or blocky.
    Vec2 size{ static_cast<float>(image->width), static_cast<float>(image->height) };
    auto surface = device_->createSurface(size);
    if (!device_->draw(surface, [&](ID2D1DeviceContext* dc) { paintImage(dc, image.get(), size, 0); })) {
        logLine("backdrop: the graphics device was reset; shown with the next cover");
        shown_ = nullptr;
        return;
    }
    auto brush = compositor_.CreateSurfaceBrush(surface);
    brush.Stretch(CompositionStretch::UniformToFill);
    auto sprite = compositor_.CreateSpriteVisual();
    sprite.Brush(brush);
    sprite.RelativeSizeAdjustment({ 1.0f, 1.0f }); // the whole island, whatever its mode
    sprite.Opacity(0.0f);
    layer_.Children().InsertAtTop(sprite);
    fadeTo(compositor_, sprite, 1.0f, fade);
    current_ = sprite;
}

void BackdropLayer::refresh()
{
    if (layer_) fadeTo(compositor_, layer_, strength(), recolorDuration(tokens()));
}

} // namespace cup::platform
