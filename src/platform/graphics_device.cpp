#include "platform/graphics_device.h"

#include <d3d11.h>

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

    auto interop = compositor.as<abi::ICompositorInterop>();
    winrt::check_hresult(interop->CreateGraphicsDevice(createD2DDevice().get(),
        reinterpret_cast<abi::ICompositionGraphicsDevice**>(winrt::put_abi(device_))));
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
        std::printf("[cup-island] graphics device lost, recreating\n");
        auto deviceInterop = device_.as<abi::ICompositionGraphicsDeviceInterop>();
        winrt::check_hresult(deviceInterop->SetRenderingDevice(createD2DDevice().get()));
        return false;
    }
    winrt::check_hresult(hr);

    // The surface may live in a shared atlas: draw relative to its offset.
    context->SetTransform(D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x), static_cast<float>(offset.y)));
    context->Clear(D2D1::ColorF(0, 0, 0, 0));
    paint(context.get());
    winrt::check_hresult(interop->EndDraw());
    return true;
}

} // namespace cup::platform
