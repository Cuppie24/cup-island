// Liquid glass's Direct3D side (decision 165): its device, the swap chain the
// compositor shows, the desktop's picture with its smaller copies, the shader.

#include "platform/liquid_glass.h"

#include "platform/liquid_shader.h"
#include "platform/log.h"

#include <d3dcompiler.h>

#include <cstring>

namespace cup::platform {
namespace {

namespace abi = ABI::Windows::UI::Composition;

winrt::com_ptr<ID3DBlob> compile(const char* entry, const char* target)
{
    winrt::com_ptr<ID3DBlob> code, errors;
    HRESULT result = D3DCompile(kLiquidShader, std::strlen(kLiquidShader), "liquid glass", nullptr, nullptr, entry, target, 0, 0, code.put(), errors.put());
    if (FAILED(result)) {
        // The compiler's own words: the line and what's wrong with it.
        if (errors) logLine("glass: liquid: shader %s: %.*s", entry, static_cast<int>(errors->GetBufferSize()), static_cast<const char*>(errors->GetBufferPointer()));
        throw winrt::hresult_error(result, L"D3DCompile");
    }
    return code;
}

} // namespace

void LiquidGlass::build(wuc::Compositor const& compositor, const char*& step)
{
    RECT at{};
    if (!GetWindowRect(layer_, &at) || at.right <= at.left || at.bottom <= at.top) throw winrt::hresult_error(E_INVALIDARG, L"the render layer has no size");
    layerOrigin_ = { at.left, at.top };
    layerSize_ = { at.right - at.left, at.bottom - at.top };

    step = "device";
    // Its own device: hardware only. On the software one a shader per frame
    // would cost what this must never cost, so there it's refused.
    winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                           device_.put(), nullptr, context_.put()));
    step = "swap chain";
    winrt::com_ptr<IDXGIDevice> dxgi;
    winrt::check_hresult(device_->QueryInterface(__uuidof(IDXGIDevice), dxgi.put_void()));
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(dxgi->GetAdapter(adapter.put()));
    winrt::com_ptr<IDXGIFactory2> factory;
    winrt::check_hresult(adapter->GetParent(__uuidof(IDXGIFactory2), factory.put_void()));
    DXGI_SWAP_CHAIN_DESC1 chain{};
    chain.Width = static_cast<UINT>(layerSize_.cx);
    chain.Height = static_cast<UINT>(layerSize_.cy);
    chain.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    chain.SampleDesc.Count = 1;
    chain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    chain.BufferCount = 2;
    chain.Scaling = DXGI_SCALING_STRETCH;
    chain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    chain.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; // clear wherever the island isn't
    winrt::check_hresult(factory->CreateSwapChainForComposition(device_.get(), &chain, nullptr, swapChain_.put()));
    winrt::com_ptr<ID3D11Texture2D> back;
    winrt::check_hresult(swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), back.put_void()));
    winrt::check_hresult(device_->CreateRenderTargetView(back.get(), nullptr, target_.put()));

    step = "surface";
    winrt::check_hresult(compositor.as<abi::ICompositorInterop>()->CreateCompositionSurfaceForSwapChain(
        swapChain_.get(), reinterpret_cast<abi::ICompositionSurface**>(winrt::put_abi(surface_))));

    step = "picture";
    D3D11_TEXTURE2D_DESC picture{};
    picture.Width = chain.Width;
    picture.Height = chain.Height;
    picture.MipLevels = 0; // all of them
    picture.ArraySize = 1;
    picture.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // the capture's
    picture.SampleDesc.Count = 1;
    picture.Usage = D3D11_USAGE_DEFAULT;
    picture.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // what GenerateMips asks for
    picture.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    winrt::check_hresult(device_->CreateTexture2D(&picture, nullptr, desktop_.put()));
    winrt::check_hresult(device_->CreateShaderResourceView(desktop_.get(), nullptr, view_.put()));

    step = "blur";
    smallSize_ = { (layerSize_.cx + 3) / 4, (layerSize_.cy + 3) / 4 };
    for (Quarter* made : { &across_, &blurred_ }) {
        D3D11_TEXTURE2D_DESC desc = picture;
        desc.Width = static_cast<UINT>(smallSize_.cx);
        desc.Height = static_cast<UINT>(smallSize_.cy);
        desc.MipLevels = 1;
        desc.MiscFlags = 0;
        winrt::check_hresult(device_->CreateTexture2D(&desc, nullptr, made->texture.put()));
        winrt::check_hresult(device_->CreateRenderTargetView(made->texture.get(), nullptr, made->target.put()));
        winrt::check_hresult(device_->CreateShaderResourceView(made->texture.get(), nullptr, made->view.put()));
    }

    step = "vertex shader";
    auto vertex = compile("vs", "vs_4_0");
    winrt::check_hresult(device_->CreateVertexShader(vertex->GetBufferPointer(), vertex->GetBufferSize(), nullptr, vertex_.put()));
    step = "pixel shader";
    auto pixel = compile("ps", "ps_4_0");
    winrt::check_hresult(device_->CreatePixelShader(pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr, pixel_.put()));

    step = "blur shader";
    auto blur = compile("blurps", "ps_4_0");
    winrt::check_hresult(device_->CreatePixelShader(blur->GetBufferPointer(), blur->GetBufferSize(), nullptr, blur_.put()));

    step = "states";
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = sizeof(LiquidParams);
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    winrt::check_hresult(device_->CreateBuffer(&buffer, nullptr, params_.put()));
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; // past the screen's edge: its last pixel
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    winrt::check_hresult(device_->CreateSamplerState(&sampler, sampler_.put()));
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.ScissorEnable = TRUE; // only the island's pixels are shaded
    raster.DepthClipEnable = TRUE;
    winrt::check_hresult(device_->CreateRasterizerState(&raster, raster_.put()));
}

} // namespace cup::platform
