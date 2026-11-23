#pragma once
// Liquid glass (decision 165): a sharp picture of the desktop behind the
// island, bent along the island's edge by a shader, in a swap chain as large
// as the render layer. Each Blob shows it through its own outline, which the
// compositor still cuts exactly; this only has to know roughly where the
// shapes are (core/liquid_glass.h), which it works out from the same springs.
// Unlike the rest of the renderer it draws per frame: whenever the desktop
// changes, and while a shape moves. Nothing while the island has no size.

#include "core/liquid_glass.h"
#include "platform/liquid_capture.h"
#include "platform/win.h"

#include <d3d11.h>
#include <dxgi1_2.h>

#include <array>
#include <chrono>
#include <functional>

namespace cup::platform {

class LiquidGlass {
public:
    // Where Windows can draw it at all (liquidCaptureOffered).
    static bool offered() { return liquidCaptureOffered(); }

    // Starts drawing for the render layer `layer`. False, with the reason in
    // the log, if anything is refused: the frosted glass stays.
    bool start(wuc::Compositor const& compositor, HWND layer);
    void stop();
    bool running() const { return running_; }

    // The picture, as large as the layer and aligned with it: a brush's surface.
    wuc::ICompositionSurface const& picture() const { return surface_; }

    // Shape `which` (0 the pill, 1 the bubble) is heading for `body` along
    // `motion` (no duration: there at once). Kept while stopped, too.
    void place(int which, Rect body, bool attached, const Motion& motion);
    void refresh() { render(); } // tokens changed

    // It stopped by itself (the GPU was reset, Windows ended the capture): show the frosted glass.
    std::function<void()> onLost;

private:
    struct Moving {
        Rect from, to;
        bool attached = true;
        Motion motion;
        std::chrono::steady_clock::time_point started{};
    };
    Rect where(const Moving& shape, std::chrono::steady_clock::time_point now) const;
    bool anyShown() const; // a shape with a size, or still shrinking to none
    void build(wuc::Compositor const& compositor, const char*& step);
    void syncCapture();    // on while there's a shape to draw, off otherwise
    void render();
    void draw();
    void lost(const char* step, winrt::hresult_error const& error);

    bool running_ = false;
    HWND layer_ = nullptr;
    POINT layerOrigin_{};
    SIZE layerSize_{};
    std::array<Moving, 2> shapes_;
    LiquidCapture capture_;
    winrt::Windows::System::DispatcherQueueTimer timer_{ nullptr }; // frames while a shape moves and the desktop doesn't
    winrt::event_token tick_{};
    wuc::ICompositionSurface surface_{ nullptr };
    winrt::com_ptr<ID3D11Device> device_;
    winrt::com_ptr<ID3D11DeviceContext> context_;
    winrt::com_ptr<IDXGISwapChain1> swapChain_;
    winrt::com_ptr<ID3D11RenderTargetView> target_;
    winrt::com_ptr<ID3D11VertexShader> vertex_;
    winrt::com_ptr<ID3D11PixelShader> pixel_;
    winrt::com_ptr<ID3D11Buffer> params_;
    winrt::com_ptr<ID3D11SamplerState> sampler_;
    winrt::com_ptr<ID3D11RasterizerState> raster_;
    winrt::com_ptr<ID3D11ShaderResourceView> view_; // of shown_
    winrt::com_ptr<ID3D11Texture2D> shown_;         // the picture view_ was made for: held, so its address isn't another's
};

} // namespace cup::platform
