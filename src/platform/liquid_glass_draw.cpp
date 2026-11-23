// Liquid glass's frames (decision 165): the desktop's part copied, blurred
// across and down, then bent into the island's shapes. Either thread, with
// LiquidGlass's lock held.

#include "platform/liquid_glass.h"

#include "platform/liquid_shader.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cup::platform {
namespace {

using Clock = std::chrono::steady_clock;

// The lip's light is the one thing here with a sharp place of its own: a
// frame or two off the compositor's edge mid-morph, it showed as a light
// band trailing the edge on a collapse (decision 165). So it's out while a
// shape moves and back once the shapes are home: at once to the eye, over
// just these few frames so it doesn't pop (220 ms at first: it read as late).
constexpr std::chrono::milliseconds kLipReturn{ 50 };

} // namespace

void LiquidGlass::pass(ID3D11RenderTargetView* target, SIZE size, D3D11_RECT scissor, ID3D11PixelShader* shader, ID3D11ShaderResourceView* source,
                       const LiquidParams& params)
{
    context_->UpdateSubresource(params_.get(), 0, nullptr, &params, 0, 0);
    context_->OMSetRenderTargets(1, &target, nullptr);
    D3D11_VIEWPORT viewport{ 0, 0, static_cast<float>(size.cx), static_cast<float>(size.cy), 0, 1 };
    context_->RSSetViewports(1, &viewport);
    context_->RSSetScissorRects(1, &scissor);
    context_->RSSetState(raster_.get());
    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vertex_.get(), nullptr, 0);
    context_->PSSetShader(shader, nullptr, 0);
    ID3D11Buffer* buffer = params_.get();
    context_->PSSetConstantBuffers(0, 1, &buffer);
    context_->PSSetShaderResources(0, 1, &source);
    ID3D11SamplerState* sampler = sampler_.get();
    context_->PSSetSamplers(0, 1, &sampler);
    context_->Draw(3, 0);
    ID3D11ShaderResourceView* none = nullptr;
    context_->PSSetShaderResources(0, 1, &none); // unbound: the next pass may draw into it
}

void LiquidGlass::draw()
{
    const Tokens& t = look_;
    auto now = Clock::now();
    bool moving = false;
    std::vector<LiquidShape> shapes;
    for (const Moving& shape : shapes_) {
        moving = moving || now - shape.started < shape.motion.duration;
        shapes.push_back(liquidShape(where(shape, now), shape.attached, shape.leftEar, shape.rightEar, t));
    }
    // How much of the lip's light: none while a shape moves, then back in.
    float lip = 1;
    for (const Moving& shape : shapes_) {
        auto home = shape.started + shape.motion.duration;
        if (shape.started == Clock::time_point{}) continue; // never moved
        lip = std::min(lip, now < home ? 0.0f : std::chrono::duration<float>(now - home) / std::chrono::duration<float>(kLipReturn));
    }
    lip = std::clamp(lip, 0.0f, 1.0f);
    moving_ = moving || lip < 1; // the thread draws the next frames (the light's return too), or sleeps
    if (!active_) { // collapsed: the frosted glass shows; nothing to draw
        moving_ = false;
        return;
    }
    if (!capture_.picture()) return; // the first picture hasn't come yet

    // Around the shapes, their ears included: all a frame shades.
    Rect bounds = liquidBounds(shapes, t.flareRadius + 2);
    D3D11_RECT scissor{ std::max(0L, static_cast<LONG>(std::floor(bounds.x))), std::max(0L, static_cast<LONG>(std::floor(bounds.y))),
                        std::min(layerSize_.cx, static_cast<LONG>(std::ceil(bounds.x + bounds.width))),
                        std::min(layerSize_.cy, static_cast<LONG>(std::ceil(bounds.y + bounds.height))) };
    bool any = scissor.right > scissor.left && scissor.bottom > scissor.top;

    // The part of the desktop the glass may show: the island's pixels and as
    // far past them as the bend and the blur reach. Only what's both on the
    // screen and in the layer.
    float blur = std::clamp(t.glassLiquidBlur, 2.0f, 16.0f);
    LONG reach = static_cast<LONG>(std::ceil(std::abs(t.glassLiquidBend) + 3 * blur)) + 24;
    POINT shift{ layerOrigin_.x - capture_.origin().x, layerOrigin_.y - capture_.origin().y }; // the layer's corner in the picture
    // Each way: from `low` to `high` in the picture, kept to where the layer and the screen overlap.
    auto span = [reach](LONG low, LONG high, LONG corner, LONG layer, float screen, UINT& from, UINT& to) {
        LONG first = std::max(0L, corner), last = std::min(static_cast<LONG>(screen), corner + layer);
        if (last <= first) return false; // the layer is off the screen
        from = static_cast<UINT>(std::clamp(low - reach + corner, first, last));
        to = static_cast<UINT>(std::clamp(high + reach + corner, first, last));
        return to > from;
    };
    D3D11_BOX part{};
    part.back = 1;
    any = any && span(scissor.left, scissor.right, shift.x, layerSize_.cx, capture_.size().x, part.left, part.right) &&
          span(scissor.top, scissor.bottom, shift.y, layerSize_.cy, capture_.size().y, part.top, part.bottom);

    LiquidParams p{};
    if (any) {
        UINT left = part.left - static_cast<UINT>(shift.x), top = part.top - static_cast<UINT>(shift.y); // in the layer
        context_->CopySubresourceRegion(desktop_.get(), 0, left, top, 0, capture_.picture(), 0, &part);
        context_->GenerateMips(view_.get());

        // Blurred, a quarter the size: across, then down. Over that part and
        // a little more, so the glass never reads a pixel the blur didn't reach.
        LONG right = static_cast<LONG>(left + (part.right - part.left)), bottom = static_cast<LONG>(top + (part.bottom - part.top));
        D3D11_RECT quarter{ std::max(0L, static_cast<LONG>(left) / 4 - 2), std::max(0L, static_cast<LONG>(top) / 4 - 2),
                            std::min(smallSize_.cx, right / 4 + 3), std::min(smallSize_.cy, bottom / 4 + 3) };
        p.quarter[0] = static_cast<float>(smallSize_.cx);
        p.quarter[1] = static_cast<float>(smallSize_.cy);
        p.blur[2] = blur / 4; // the Gaussian's sigma, in the quarter picture's pixels
        p.blur[0] = 1.0f / p.quarter[0], p.blur[1] = 0, p.blur[3] = 2; // across, from the picture's quarter-size copy
        pass(across_.target.get(), smallSize_, quarter, blur_.get(), view_.get(), p);
        p.blur[0] = 0, p.blur[1] = 1.0f / p.quarter[1], p.blur[3] = 0; // down
        pass(blurred_.target.get(), smallSize_, quarter, blur_.get(), across_.view.get(), p);
    }

    ID3D11RenderTargetView* target = target_.get();
    context_->OMSetRenderTargets(1, &target, nullptr);
    const float clear[4]{ 0, 0, 0, 0 };
    context_->ClearRenderTargetView(target, clear);
    if (any) {
        p.screen[0] = static_cast<float>(layerSize_.cx);
        p.screen[1] = static_cast<float>(layerSize_.cy);
        for (std::size_t i = 0; i < 2; ++i) {
            const Rect& f = shapes[i].frame;
            p.shape[i][0] = f.x, p.shape[i][1] = f.y, p.shape[i][2] = f.width, p.shape[i][3] = f.height;
            p.radius[i] = shapes[i].radius;
            p.ears[2 * i] = shapes[i].leftEar, p.ears[2 * i + 1] = shapes[i].rightEar;
            p.tops[i] = shapes[i].top;
        }
        p.lens[1] = std::max(t.glassLiquidRim, 0.0f);
        p.lens[2] = t.glassLiquidBend;
        p.lens[3] = std::clamp(t.glassLiquidDim, 0.0f, 1.0f);
        p.light[0] = std::max(t.glassLiquidSaturation, 0.0f);
        p.light[1] = std::clamp(t.glassLiquidLipOpacity, 0.0f, 1.0f) * lip * lip * (3 - 2 * lip); // eased
        pass(target, layerSize_, scissor, pixel_.get(), blurred_.view.get(), p);
    }
    // No waiting for the screen: a frame the compositor doesn't get to is replaced by the next.
    winrt::check_hresult(swapChain_->Present(0, 0));
    if (any && !ready_) { // the first picture since it was turned on: the island may show it
        ready_ = true;
        queue_.TryEnqueue([this] {
            if (onReady) onReady();
        });
    }
}

} // namespace cup::platform
