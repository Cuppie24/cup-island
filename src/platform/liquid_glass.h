#pragma once
// Liquid glass (decision 165): a sharp picture of the desktop behind the
// island, bent along the island's edge by a shader, in a swap chain as large
// as the render layer. Each Blob shows it through its own outline, which the
// compositor still cuts exactly; this only has to know roughly where the
// shapes are (core/liquid_glass.h), which it works out from the same springs.
// Unlike the rest of the renderer it draws per frame: whenever the desktop
// changes, and while a shape moves. Nothing while the island has no size.
// While a shape moves the frames come from a thread of its own, one a screen
// refresh: the UI thread is busy just then (a collapse builds the pill's
// content), and frames drawn from it came late, the rim trailing the edge.

#include "core/liquid_glass.h"
#include "core/tokens.h"
#include "platform/liquid_capture.h"
#include "platform/win.h"

#include <d3d11.h>
#include <dxgi1_2.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace cup::platform {

struct LiquidParams; // liquid_shader.h

class LiquidGlass {
public:
    LiquidGlass() = default;
    ~LiquidGlass(); // its thread is stopped first
    LiquidGlass(const LiquidGlass&) = delete;
    LiquidGlass& operator=(const LiquidGlass&) = delete;

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
    // `leftEar`, `rightEar`: it has that flared top corner (Blob::setEars).
    void place(int which, Rect body, bool attached, bool leftEar, bool rightEar, const Motion& motion);
    void refresh(); // tokens changed
    // Drawn or not (decision 172: only while a card is open). Off, nothing is
    // captured or drawn, the island is in screenshots again, and the last
    // picture stays as it was. On, onReady says when a fresh one is up.
    void setActive(bool on);
    std::function<void()> onReady; // on the UI thread

    // It stopped by itself (the GPU was reset, Windows ended the capture): show the frosted glass.
    std::function<void()> onLost;

private:
    struct Moving {
        Rect from, to;
        bool attached = true;
        bool leftEar = true, rightEar = true;
        Motion motion;
        std::chrono::steady_clock::time_point started{};
    };
    Rect where(const Moving& shape, std::chrono::steady_clock::time_point now) const;
    bool anyShown() const; // a shape with a size, or still shrinking to none
    void build(wuc::Compositor const& compositor, const char*& step);
    // The UI thread's: the capture on or off, then a frame. Errors end it (lost).
    void render();
    void syncCapture(); // on while there's a shape to draw, off otherwise; lock_ held
    // Shape `which` heads for `body`; false if it already was. lock_ held.
    bool move(int which, Rect body, bool attached, bool leftEar, bool rightEar, const Motion& motion);
    void draw();        // one frame; lock_ held, either thread
    void lost(const char* step, winrt::hresult_error const& error); // UI thread, lock_ not held
    void tick();        // the thread's: frames while moving_

    // Everything below is shared with the thread and touched only with lock_
    // held (Direct3D's context takes one thread at a time).
    std::mutex lock_;
    std::condition_variable wake_;
    std::thread ticker_;
    bool active_ = false; // setActive()
    bool ready_ = false;  // a picture has been drawn since it was last turned on
    bool moving_ = false; // a shape is on its way: the thread draws
    bool quit_ = false;
    Tokens look_;         // tokens(), copied on the UI thread: the thread never reads the live ones
    winrt::Windows::System::DispatcherQueue queue_{ nullptr }; // the UI thread's: where the thread hands back

    bool running_ = false;
    // The UI thread's only. Switching the capture on or off lets Windows run
    // this thread's messages, so place(), refresh() and render() can be
    // called from inside syncCapture(), lock_ already held by this thread
    // (capture_.switching()). They note what's wanted and leave; syncCapture()
    // looks again before it returns (decision 174).
    bool again_ = false;
    HWND layer_ = nullptr;
    POINT layerOrigin_{};
    SIZE layerSize_{};
    std::array<Moving, 2> shapes_;
    LiquidCapture capture_;
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
    // The render layer's part of the desktop's picture, copied from the
    // capture each frame, with smaller copies of itself (mip levels): what
    // the shader reads, blurred for free.
    winrt::com_ptr<ID3D11Texture2D> desktop_;
    winrt::com_ptr<ID3D11ShaderResourceView> view_;
    // ...and that picture blurred, a quarter the size: across into the
    // first, then down into the second, which is what the glass shows.
    struct Quarter {
        winrt::com_ptr<ID3D11Texture2D> texture;
        winrt::com_ptr<ID3D11RenderTargetView> target;
        winrt::com_ptr<ID3D11ShaderResourceView> view;
    };
    Quarter across_, blurred_;
    SIZE smallSize_{};
    winrt::com_ptr<ID3D11PixelShader> blur_;
    // One pass: `shader` over `scissor` of `target` (as large as `size`), reading `source`.
    void pass(ID3D11RenderTargetView* target, SIZE size, D3D11_RECT scissor, ID3D11PixelShader* shader, ID3D11ShaderResourceView* source,
              const LiquidParams& params);
};

} // namespace cup::platform
