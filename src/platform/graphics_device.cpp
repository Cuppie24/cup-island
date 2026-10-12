#include "platform/graphics_device.h"

#include "core/tokens.h"
#include "platform/log.h"

#include <d3d11.h>
#include <dwrite_1.h>

#include <cmath>
#include <cstdio>

namespace cup::platform {
namespace {

namespace abi = ABI::Windows::UI::Composition;

winrt::com_ptr<ID3D11Device> createD3DDevice()
{
    winrt::com_ptr<ID3D11Device> device;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // required for Direct2D interop
    for (D3D_DRIVER_TYPE type : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP }) {
        if (SUCCEEDED(D3D11CreateDevice(nullptr, type, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                                        device.put(), nullptr, nullptr)))
            return device;
    }
    winrt::throw_hresult(E_FAIL);
}

bool isDeviceLost(HRESULT hr)
{
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET;
}

} // namespace

winrt::com_ptr<ID2D1Device> GraphicsDevice::createD2DDevice()
{
    auto dxgiDevice = createD3DDevice().as<IDXGIDevice>();
    winrt::com_ptr<ID2D1Device> d2dDevice;
    winrt::check_hresult(d2dFactory_->CreateDevice(dxgiDevice.get(), d2dDevice.put()));
    return d2dDevice;
}

void GraphicsDevice::create(wuc::Compositor const& compositor)
{
    winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr, d2dFactory_.put_void()));
    winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                             reinterpret_cast<IUnknown**>(dwrite_.put())));
    updateTextParams();

    auto interop = compositor.as<abi::ICompositorInterop>();
    winrt::check_hresult(interop->CreateGraphicsDevice(createD2DDevice().get(),
        reinterpret_cast<abi::ICompositionGraphicsDevice**>(winrt::put_abi(device_))));
}

// The user's own gamma, no ClearType (the surfaces are transparent),
// horizontal-only smoothing (NATURAL), and grayscale contrast from the
// textContrast token: higher darkens thin strokes on light-on-dark text.
void GraphicsDevice::updateTextParams()
{
    float contrast = tokens().textContrast;
    if (contrast == textParamsContrast_ && textParams_) return;
    textParamsContrast_ = contrast;
    winrt::com_ptr<IDWriteRenderingParams> defaults;
    if (FAILED(dwrite_->CreateRenderingParams(defaults.put()))) return;
    textParams_ = nullptr;
    if (auto factory1 = dwrite_.try_as<IDWriteFactory1>()) {
        winrt::com_ptr<IDWriteRenderingParams1> params;
        if (SUCCEEDED(factory1->CreateCustomRenderingParams(defaults->GetGamma(), defaults->GetEnhancedContrast(), contrast, 0.0f,
                                                            DWRITE_PIXEL_GEOMETRY_FLAT, DWRITE_RENDERING_MODE_NATURAL, params.put())))
            textParams_ = params;
    }
    if (!textParams_) // no IDWriteFactory1: no separate grayscale contrast
        dwrite_->CreateCustomRenderingParams(defaults->GetGamma(), contrast, 0.0f, DWRITE_PIXEL_GEOMETRY_FLAT,
                                             DWRITE_RENDERING_MODE_NATURAL, textParams_.put());
}

wuc::CompositionDrawingSurface GraphicsDevice::createSurface(Vec2 size)
{
    winrt::Windows::Foundation::Size pixels{ std::ceil(std::max(size.x, 1.0f)), std::ceil(std::max(size.y, 1.0f)) };
    return device_.CreateDrawingSurface(pixels,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        winrt::Windows::Graphics::DirectX::DirectXAlphaMode::Premultiplied);
}

bool GraphicsDevice::draw(wuc::CompositionDrawingSurface const& surface,
                          const std::function<void(ID2D1DeviceContext*)>& paint)
{
    auto interop = surface.as<abi::ICompositionDrawingSurfaceInterop>();
    winrt::com_ptr<ID2D1DeviceContext> context;
    POINT offset{};
    HRESULT hr = interop->BeginDraw(nullptr, __uuidof(ID2D1DeviceContext), context.put_void(), &offset);
    if (isDeviceLost(hr)) {
        // GPU reset or driver update: give Composition a fresh device; all
        // surfaces lose their pixels, so everything is redrawn.
        logLine("graphics device lost, recreating");
        auto deviceInterop = device_.as<abi::ICompositionGraphicsDeviceInterop>();
        winrt::check_hresult(deviceInterop->SetRenderingDevice(createD2DDevice().get()));
        return false;
    }
    winrt::check_hresult(hr);

    // The surface may live in a shared atlas: draw relative to its offset.
    context->SetTransform(D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x), static_cast<float>(offset.y)));
    // Surfaces are transparent, so text is grayscale-antialiased (ClearType
    // needs an opaque background). NATURAL smooths horizontally only: crisper
    // than the default symmetric mode at small sizes.
    context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    updateTextParams(); // textContrast is live-tunable
    if (textParams_) context->SetTextRenderingParams(textParams_.get());
    context->Clear(D2D1::ColorF(0, 0, 0, 0));
    paint(context.get());
    winrt::check_hresult(interop->EndDraw());
    return true;
}

} // namespace cup::platform
