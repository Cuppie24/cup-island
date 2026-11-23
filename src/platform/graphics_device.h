#pragma once
// Direct2D/DirectWrite drawing into Composition surfaces: how text, images
// and icon glyphs get pixels. One device for the whole app.

#include "platform/win.h"

#include <d2d1_1.h>
#include <dwrite.h>

#include <cstdint>
#include <functional>

namespace cup::platform {

class GraphicsDevice {
public:
    // Hardware D3D11, falling back to WARP (software). Throws on failure.
    void create(wuc::Compositor const& compositor);

    // A GPU reset found here rebuilds the device first (decision 164): every
    // existing surface lost its pixels, which generation() tells the caller.
    wuc::CompositionDrawingSurface createSurface(Vec2 size);

    // Clears the surface to transparent, then lets `paint` draw at (0, 0).
    // Returns false if the GPU was lost: the device is rebuilt, and the
    // caller should redraw every surface (they all lost their pixels).
    // `bigText`: its text is big (a timer), drawn as smooth outlines rather
    // than with small text's pixel-row-snapped hinting (decisions 107, 154).
    bool draw(wuc::CompositionDrawingSurface const& surface, const std::function<void(ID2D1DeviceContext*)>& paint, bool bigText = false);

    IDWriteFactory* dwrite() const { return dwrite_.get(); }
    ID2D1Factory* d2dFactory() const { return d2dFactory_.get(); } // for a path's geometry (a wavy bar, decision 167)

    // Goes up each time the device is rebuilt after a GPU reset, wherever it
    // was found: a caller that sees it change redraws every surface it has.
    std::uint64_t generation() const { return generation_; }

private:
    winrt::com_ptr<ID2D1Device> createD2DDevice();
    void recover(); // a fresh device for Composition after a GPU reset
    std::uint64_t generation_ = 0;

    winrt::com_ptr<ID2D1Factory1> d2dFactory_;
    winrt::com_ptr<IDWriteFactory> dwrite_;
    winrt::com_ptr<IDWriteRenderingParams> textParams_; // crisp, fuller small text (decision 024)
    winrt::com_ptr<IDWriteRenderingParams> outlineParams_; // big text, smoothed both ways (decision 107)
    float textParamsContrast_ = -1;                     // the textContrast token they were built for
    void updateTextParams();                            // rebuilds them when textContrast changed
    wuc::CompositionGraphicsDevice device_{ nullptr };
};

} // namespace cup::platform
