#include "platform/liquid_glass.h"

#include "core/tokens.h"
#include "platform/glass.h"
#include "platform/liquid_shader.h"
#include "platform/log.h"


#include <dwmapi.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace cup::platform {
namespace {

using Clock = std::chrono::steady_clock;

} // namespace

LiquidGlass::~LiquidGlass()
{
    try {
        stop(); // a thread still joinable when it's destroyed would end the process
    } catch (...) { // at exit, with Windows' runtime already going: nothing to do about it
    }
}

bool LiquidGlass::start(wuc::Compositor const& compositor, HWND layer)
{
    stop();
    layer_ = layer;
    const char* step = "layer";
    try {
        build(compositor, step);
        step = "thread";
        queue_ = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        look_ = tokens();
        quit_ = false;
        moving_ = false;
        active_ = false;
        ready_ = false;
        running_ = true;
        ticker_ = std::thread([this] { tick(); });
        step = "capture";
        std::lock_guard<std::mutex> held(lock_);
        syncCapture();
    } catch (winrt::hresult_error const& e) {
        logGlassError("liquid: can't start, the frosted glass stays", step, e);
        stop();
        return false;
    } catch (std::exception const& e) { // no thread to be had
        logLine("glass: liquid: can't start, the frosted glass stays (%s: %s)", step, e.what());
        stop();
        return false;
    }
    logLine("glass: liquid: started, a layer %ldx%ld at (%ld, %ld)", layerSize_.cx, layerSize_.cy, layerOrigin_.x, layerOrigin_.y);
    return true;
}

void LiquidGlass::stop()
{
    bool was = running_;
    {
        std::lock_guard<std::mutex> held(lock_);
        running_ = false;
        moving_ = false;
        quit_ = true;
    }
    wake_.notify_all();
    if (ticker_.joinable()) ticker_.join(); // not holding lock_: it may be waiting for it
    std::lock_guard<std::mutex> held(lock_);
    capture_.stop();
    view_ = nullptr;
    desktop_ = nullptr;
    across_ = {};
    blurred_ = {};
    blur_ = nullptr;
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

void LiquidGlass::tick()
{
    // The thread: asleep until a shape moves, then a frame after each of the
    // compositor's own (DwmFlush waits for one), until the shapes are home.
    std::unique_lock<std::mutex> held(lock_);
    while (!quit_) {
        wake_.wait(held, [this] { return quit_ || moving_; });
        if (quit_) break;
        held.unlock();
        DwmFlush();
        held.lock();
        if (quit_ || !moving_) continue;
        try {
            draw();
        } catch (winrt::hresult_error const& e) {
            // Ended on the UI thread, which owns the visuals: nothing thrown out of here (decision 158).
            moving_ = false;
            winrt::hresult code = e.code();
            winrt::hstring message = e.message();
            queue_.TryEnqueue([this, code, message] { lost("frame (its thread)", winrt::hresult_error(code, message)); });
            continue;
        }
        // Home: one more frame from the UI thread, which also pauses the capture if the island is gone.
        if (!moving_) queue_.TryEnqueue([this] { render(); });
    }
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

void LiquidGlass::place(int which, Rect body, bool attached, bool leftEar, bool rightEar, const Motion& motion)
{
    {
        std::lock_guard<std::mutex> held(lock_);
        Moving& shape = shapes_[static_cast<std::size_t>(which)];
        if (body == shape.to && attached == shape.attached && leftEar == shape.leftEar && rightEar == shape.rightEar) return;
        shape.leftEar = leftEar;
        shape.rightEar = rightEar;
        auto now = Clock::now();
        shape.from = where(shape, now); // an interrupted morph carries on from where it got to
        shape.to = body;
        shape.attached = attached;
        shape.motion = motion;
        shape.started = now;
    }
    render();
}

void LiquidGlass::refresh()
{
    {
        std::lock_guard<std::mutex> held(lock_);
        look_ = tokens();
    }
    render();
}

void LiquidGlass::setActive(bool on)
{
    {
        std::lock_guard<std::mutex> held(lock_);
        if (on == active_) return;
        active_ = on;
        ready_ = false;
    }
    render(); // starts or stops the capture
}

void LiquidGlass::syncCapture()
{
    bool shown = active_ && anyShown();
    if (shown && !capture_.running()) {
        capture_.start(layer_, device_.get(), lock_, [this] { render(); });
        logLine("glass: liquid: capturing the screen (%.0fx%.0f); the island is left out of screenshots", capture_.size().x, capture_.size().y);
    } else if (!shown && capture_.running()) {
        // Hidden (a fullscreen app, decision 032): nothing is captured, nothing costs.
        capture_.stop();
        logLine("glass: liquid: no open card to draw for: the capture is paused");
    }
}

void LiquidGlass::render()
{
    // The UI thread's, called by the app or by Windows (a new picture):
    // nothing thrown back (decision 158).
    bool moving = false;
    try {
        std::lock_guard<std::mutex> held(lock_);
        if (!running_) return;
        syncCapture();
        draw();
        moving = moving_;
    } catch (winrt::hresult_error const& e) {
        lost("frame", e); // not holding lock_
        return;
    }
    if (moving) wake_.notify_one(); // the thread takes it from here
}

} // namespace cup::platform
