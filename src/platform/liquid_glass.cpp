#include "platform/liquid_glass.h"

#include "core/tokens.h"
#include "platform/glass.h"
#include "platform/liquid_shader.h"
#include "platform/log.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace cup::platform {
namespace {

namespace abi = ABI::Windows::UI::Composition;
using Clock = std::chrono::steady_clock;

// While a shape moves and the desktop is still, frames come from a timer:
// about one a screen refresh. The outline is the compositor's, at its own rate.
constexpr std::chrono::milliseconds kFrame{ 8 };

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

bool LiquidGlass::start(wuc::Compositor const& compositor, HWND layer)
{
    stop();
    layer_ = layer;
    const char* step = "layer";
    try {
        build(compositor, step);
        step = "timer";
        timer_ = winrt::Windows::System::DispatcherQueue::GetForCurrentThread().CreateTimer();
        timer_.Interval(kFrame);
        timer_.IsRepeating(true);
        tick_ = timer_.Tick([this](auto const&, auto const&) { render(); });
        running_ = true;
        step = "capture";
        syncCapture();
    } catch (winrt::hresult_error const& e) {
        logGlassError("liquid: can't start, the frosted glass stays", step, e);
        stop();
        return false;
    }
    logLine("glass: liquid: started, a layer %ldx%ld at (%ld, %ld)", layerSize_.cx, layerSize_.cy, layerOrigin_.x, layerOrigin_.y);
    return true;
}

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

    step = "vertex shader";
    auto vertex = compile("vs", "vs_4_0");
    winrt::check_hresult(device_->CreateVertexShader(vertex->GetBufferPointer(), vertex->GetBufferSize(), nullptr, vertex_.put()));
    step = "pixel shader";
    auto pixel = compile("ps", "ps_4_0");
    winrt::check_hresult(device_->CreatePixelShader(pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr, pixel_.put()));

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

void LiquidGlass::stop()
{
    bool was = running_;
    running_ = false;
    if (timer_) {
        timer_.Stop();
        timer_.Tick(tick_);
        timer_ = nullptr;
    }
    capture_.stop();
    view_ = nullptr;
    shown_ = nullptr;
    surface_ = nullptr;
    target_ = nullptr;
    swapChain_ = nullptr;
    vertex_ = nullptr;
    pixel_ = nullptr;
    params_ = nullptr;
    sampler_ = nullptr;
    raster_ = nullptr;
    context_ = nullptr;
    device_ = nullptr;
    if (was) logLine("glass: liquid: stopped");
}

void LiquidGlass::lost(const char* step, winrt::hresult_error const& error)
{
    logGlassError("liquid: stopped by an error, the frosted glass instead", step, error);
    stop();
    if (onLost) onLost();
}

Rect LiquidGlass::where(const Moving& shape, Clock::time_point now) const
{
    return morphRect(shape.from, shape.to, shape.motion, std::chrono::duration_cast<Ms>(now - shape.started));
}

bool LiquidGlass::anyShown() const
{
    auto now = Clock::now();
    for (const Moving& shape : shapes_) {
        Rect at = where(shape, now);
        if (at.width > 0 && at.height > 0) return true;
    }
    return false;
}

void LiquidGlass::place(int which, Rect body, bool attached, const Motion& motion)
{
    Moving& shape = shapes_[static_cast<std::size_t>(which)];
    if (body == shape.to && attached == shape.attached) return;
    auto now = Clock::now();
    shape.from = where(shape, now); // an interrupted morph carries on from where it got to
    shape.to = body;
    shape.attached = attached;
    shape.motion = motion;
    shape.started = now;
    if (!running_) return;
    try {
        syncCapture();
    } catch (winrt::hresult_error const& e) {
        lost("capture", e);
        return;
    }
    render();
}

void LiquidGlass::syncCapture()
{
    bool shown = anyShown();
    if (shown && !capture_.running()) {
        capture_.start(layer_, device_.get(), [this] { render(); });
        logLine("glass: liquid: capturing the screen (%.0fx%.0f); the island is left out of screenshots", capture_.size().x, capture_.size().y);
    } else if (!shown && capture_.running()) {
        // Hidden (a fullscreen app, decision 032): nothing is captured, nothing costs.
        capture_.stop();
        view_ = nullptr;
        shown_ = nullptr;
        logLine("glass: liquid: the island has no size: the capture is paused");
    }
}

void LiquidGlass::render()
{
    if (!running_) return;
    // Called from Windows (a frame, the timer): nothing thrown back (decision 158).
    try {
        syncCapture();
        draw();
    } catch (winrt::hresult_error const& e) {
        lost("frame", e);
    }
}

void LiquidGlass::draw()
{
    const Tokens& t = tokens();
    auto now = Clock::now();
    bool moving = false;
    std::vector<LiquidShape> shapes;
    for (const Moving& shape : shapes_) {
        moving = moving || now - shape.started < shape.motion.duration;
        shapes.push_back(liquidShape(where(shape, now), shape.attached, t));
    }
    if (moving) timer_.Start();
    else timer_.Stop();
    if (!capture_.picture()) return; // the first picture hasn't come yet

    if (capture_.picture() != shown_.get()) {
        view_ = nullptr;
        winrt::check_hresult(device_->CreateShaderResourceView(capture_.picture(), nullptr, view_.put()));
        shown_.copy_from(capture_.picture());
    }
    ID3D11RenderTargetView* target = target_.get();
    context_->OMSetRenderTargets(1, &target, nullptr);
    const float clear[4]{ 0, 0, 0, 0 };
    context_->ClearRenderTargetView(target, clear);

    // Around the shapes, their ears included: all a frame shades.
    Rect bounds = liquidBounds(shapes, t.flareRadius + 2);
    D3D11_RECT scissor{ std::max(0L, static_cast<LONG>(std::floor(bounds.x))), std::max(0L, static_cast<LONG>(std::floor(bounds.y))),
                        std::min(layerSize_.cx, static_cast<LONG>(std::ceil(bounds.x + bounds.width))),
                        std::min(layerSize_.cy, static_cast<LONG>(std::ceil(bounds.y + bounds.height))) };
    if (scissor.right > scissor.left && scissor.bottom > scissor.top) {
        LiquidParams p{};
        p.screen[0] = capture_.size().x;
        p.screen[1] = capture_.size().y;
        p.screen[2] = static_cast<float>(layerOrigin_.x - capture_.origin().x);
        p.screen[3] = static_cast<float>(layerOrigin_.y - capture_.origin().y);
        for (std::size_t i = 0; i < 2; ++i) {
            const Rect& f = shapes[i].frame;
            p.shape[i][0] = f.x, p.shape[i][1] = f.y, p.shape[i][2] = f.width, p.shape[i][3] = f.height;
            p.radius[i] = shapes[i].radius;
        }
        p.radius[2] = std::max(t.glassRimWidth, 1.0f) * 1.5f; // the highlight: a little wider than the rim's line
        p.lens[0] = std::max(t.glassLiquidBlur, 0.0f);
        p.lens[1] = std::max(t.glassLiquidRim, 0.0f);
        p.lens[2] = t.glassLiquidBend;
        p.lens[3] = std::clamp(t.glassLiquidDim, 0.0f, 1.0f);
        p.light[0] = std::max(t.glassLiquidSaturation, 0.0f);
        p.light[1] = std::clamp(t.glassLiquidHighlightOpacity, 0.0f, 1.0f);
        context_->UpdateSubresource(params_.get(), 0, nullptr, &p, 0, 0);

        D3D11_VIEWPORT viewport{ 0, 0, static_cast<float>(layerSize_.cx), static_cast<float>(layerSize_.cy), 0, 1 };
        context_->RSSetViewports(1, &viewport);
        context_->RSSetScissorRects(1, &scissor);
        context_->RSSetState(raster_.get());
        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertex_.get(), nullptr, 0);
        context_->PSSetShader(pixel_.get(), nullptr, 0);
        ID3D11Buffer* params = params_.get();
        context_->PSSetConstantBuffers(0, 1, &params);
        ID3D11ShaderResourceView* view = view_.get();
        context_->PSSetShaderResources(0, 1, &view);
        ID3D11SamplerState* sampler = sampler_.get();
        context_->PSSetSamplers(0, 1, &sampler);
        context_->Draw(3, 0);
        view = nullptr;
        context_->PSSetShaderResources(0, 1, &view); // the picture goes back to the capture unbound
    }
    // No waiting for the screen: a frame the compositor doesn't get to is replaced by the next.
    winrt::check_hresult(swapChain_->Present(0, 0));
}

} // namespace cup::platform
