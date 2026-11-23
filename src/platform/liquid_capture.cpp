#include "platform/liquid_capture.h"

#include "platform/glass.h"
#include "platform/log.h"

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <utility>

namespace cup::platform {
namespace {

namespace wgc = winrt::Windows::Graphics::Capture;
namespace wgd = winrt::Windows::Graphics::DirectX;

constexpr DWORD kLeftOutOfCaptures = 0x00000011; // WDA_EXCLUDEFROMCAPTURE (Windows 10 2004); older SDKs don't name it
constexpr wchar_t kSession[] = L"Windows.Graphics.Capture.GraphicsCaptureSession";

} // namespace

bool liquidCaptureOffered()
{
    using winrt::Windows::Foundation::Metadata::ApiInformation;
    try {
        // IsBorderRequired is Windows 11's: before it, Windows draws a yellow
        // border round whatever is captured, the whole monitor here.
        bool offered = ApiInformation::IsTypePresent(kSession) && ApiInformation::IsPropertyPresent(kSession, L"IsBorderRequired") &&
                       wgc::GraphicsCaptureSession::IsSupported();
        logLine("glass: liquid: %s", offered ? "offered (Windows can capture a monitor without a border)" : "not offered on this Windows");
        return offered;
    } catch (winrt::hresult_error const& e) {
        logGlassError("liquid: not offered", "capture support", e);
        return false;
    }
}

void LiquidCapture::start(HWND window, ID3D11Device* device, std::function<void()> onFrame)
{
    stop();
    window_ = window;
    onFrame_ = std::move(onFrame);
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{ sizeof info };
    if (!GetMonitorInfoW(monitor, &info)) throw winrt::hresult_error(HRESULT_FROM_WIN32(GetLastError()), L"GetMonitorInfo");
    origin_ = { info.rcMonitor.left, info.rcMonitor.top };

    // First: a picture with the island in it would be bent into the island.
    if (!SetWindowDisplayAffinity(window, kLeftOutOfCaptures)) throw winrt::hresult_error(HRESULT_FROM_WIN32(GetLastError()), L"SetWindowDisplayAffinity");
    leftOut_ = true;

    winrt::com_ptr<IDXGIDevice> dxgi;
    winrt::check_hresult(device->QueryInterface(__uuidof(IDXGIDevice), dxgi.put_void()));
    winrt::com_ptr<::IInspectable> boxed;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), boxed.put()));
    device_ = boxed.as<wgd::Direct3D11::IDirect3DDevice>();

    auto factory = winrt::get_activation_factory<wgc::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    wgc::GraphicsCaptureItem item{ nullptr };
    winrt::check_hresult(factory->CreateForMonitor(monitor, winrt::guid_of<wgc::IGraphicsCaptureItem>(), winrt::put_abi(item)));
    size_ = item.Size();
    // Two frames: the one being shown is kept while the next arrives.
    pool_ = wgc::Direct3D11CaptureFramePool::Create(device_, wgd::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size_);
    arrived_ = pool_.FrameArrived(winrt::auto_revoke, [this](wgc::Direct3D11CaptureFramePool const& pool, auto const&) { take(pool); });
    session_ = pool_.CreateCaptureSession(item);
    try {
        session_.IsCursorCaptureEnabled(false); // the pointer isn't behind the glass
    } catch (winrt::hresult_error const& e) {
        logGlassError("liquid: the pointer stays in the picture", "IsCursorCaptureEnabled", e);
    }
    try {
        session_.IsBorderRequired(false);
    } catch (winrt::hresult_error const& e) {
        logGlassError("liquid: Windows may draw a border round the screen", "IsBorderRequired", e);
    }
    session_.StartCapture();
}

void LiquidCapture::stop()
{
    arrived_.revoke();
    try {
        if (session_) session_.Close();
        if (frame_) frame_.Close();
        if (pool_) pool_.Close();
    } catch (winrt::hresult_error const& e) {
        logGlassError("liquid: the capture didn't close cleanly", "close", e);
    }
    session_ = nullptr;
    frame_ = nullptr;
    pool_ = nullptr;
    device_ = nullptr;
    picture_ = nullptr;
    if (leftOut_ && !SetWindowDisplayAffinity(window_, WDA_NONE))
        logLine("glass: liquid: the island stays out of screenshots (error %lu)", GetLastError());
    leftOut_ = false;
}

void LiquidCapture::take(wgc::Direct3D11CaptureFramePool const& pool)
{
    // Called by Windows: nothing may be thrown back at it (decision 158).
    try {
        wgc::Direct3D11CaptureFrame latest{ nullptr };
        while (auto next = pool.TryGetNextFrame()) { // only the newest matters
            if (latest) latest.Close();
            latest = next;
        }
        if (!latest) return;
        auto access = latest.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<ID3D11Texture2D> texture;
        winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));
        if (frame_) frame_.Close(); // back to the pool
        frame_ = latest;
        picture_ = texture;
        if (auto now = latest.ContentSize(); now.Width != size_.Width || now.Height != size_.Height) {
            // Another resolution: the next frames at it. (The render layer is resized too, which restarts all this.)
            size_ = now;
            pool.Recreate(device_, wgd::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size_);
        }
    } catch (winrt::hresult_error const& e) {
        logGlassError("liquid: a picture of the desktop was lost", "frame", e);
        return;
    }
    if (onFrame_) onFrame_();
}

} // namespace cup::platform
