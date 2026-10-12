#pragma once
// Direct2D/DirectWrite drawing into Composition surfaces: how text, images
// and icon glyphs get pixels. One device for the whole app.

#include "platform/win.h"

#include <d2d1_1.h>
#include <dwrite.h>

#include <functional>

namespace cup::platform {

class GraphicsDevice {
public:
    // Hardware D3D11, falling back to WARP (software). Throws on failure.
    void create(wuc::Compositor const& compositor);

    wuc::CompositionDrawingSurface createSurface(Vec2 size);

    // Clears the surface to transparent, then lets `paint` draw at (0, 0).
    // Returns false if the GPU was lost: the device is rebuilt, and the
    // caller should redraw every surface (they all lost their pixels).
    bool draw(wuc::CompositionDrawingSurface const& surface, const std::function<void(ID2D1DeviceContext*)>& paint);

    IDWriteFactory* dwrite() const { return dwrite_.get(); }

private:
    winrt::com_ptr<ID2D1Device> createD2DDevice();

    winrt::com_ptr<ID2D1Factory1> d2dFactory_;
    winrt::com_ptr<IDWriteFactory> dwrite_;
    winrt::com_ptr<IDWriteRenderingParams> textParams_; // crisp, fuller small text (decision 024)
    float textParamsContrast_ = -1;                     // the textContrast token they were built for
    void updateTextParams();                            // rebuilds them when textContrast changed
    wuc::CompositionGraphicsDevice device_{ nullptr };
};

} // namespace cup::platform
