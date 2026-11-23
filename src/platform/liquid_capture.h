#pragma once
// The desktop's picture for liquid glass (decision 165): Windows' own screen
// capture of one monitor, as textures on our device. The island's window is
// left out of it, or the glass would show itself; that also leaves it out of
// screenshots, recordings and screen shares while this runs.

#include "platform/win.h"

#include <d3d11.h>
#include <winrt/Windows.Graphics.Capture.h>

#include <functional>

namespace cup::platform {

// Whether this Windows can capture a monitor without drawing a border round
// it (Windows 11). Looked up at runtime (decision 015).
bool liquidCaptureOffered();

class LiquidCapture {
public:
    // Captures `window`'s monitor on `device`, `window` left out. `onFrame`
    // runs on this thread (it needs a DispatcherQueue) whenever the desktop
    // changed. Throws winrt::hresult_error; stop() undoes a half-made start.
    void start(HWND window, ID3D11Device* device, std::function<void()> onFrame);
    void stop(); // ...and lets `window` into captures again
    bool running() const { return session_ != nullptr; }

    ID3D11Texture2D* picture() const { return picture_.get(); } // the latest; none until the first frame
    Vec2 size() const { return { static_cast<float>(size_.Width), static_cast<float>(size_.Height) }; }
    POINT origin() const { return origin_; } // the monitor's top left, screen pixels

private:
    void take(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& pool);

    HWND window_ = nullptr;
    bool leftOut_ = false;
    POINT origin_{};
    winrt::Windows::Graphics::SizeInt32 size_{};
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device_{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool pool_{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session_{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker arrived_;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame_{ nullptr }; // kept: it owns picture_
    winrt::com_ptr<ID3D11Texture2D> picture_;
    std::function<void()> onFrame_;
};

} // namespace cup::platform
