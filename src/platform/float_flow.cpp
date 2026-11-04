#include "platform/float_flow.h"

#include "platform/log.h"
#include "platform/render_layer.h"

#include "core/layout.h"
#include "core/motion.h"
#include "core/tokens.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace cup::platform {
namespace {

constexpr wchar_t kClass[] = L"CupIslandFlow";

int px(float v) { return static_cast<int>(std::lround(v)); }

Rect around(Rect a, Rect b, float margin)
{
    float left = std::min(a.x, b.x) - margin, top = std::min(a.y, b.y) - margin;
    float right = std::max(a.x + a.width, b.x + b.width) + margin, bottom = std::max(a.y + a.height, b.y + b.height) + margin;
    return { std::floor(left), std::floor(top), std::ceil(right - left), std::ceil(bottom - top) };
}

Scene sceneAt(Rect r)
{
    Scene s;
    s.size = { r.width, r.height };
    s.offset = { r.x, r.y };
    return s;
}

} // namespace

bool FloatFlow::make(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device)
{
    if (built_) return true;
    WNDCLASSEXW windowClass{ sizeof(windowClass) };
    windowClass.lpfnWndProc = DefWindowProcW; // input never reaches it (WS_EX_TRANSPARENT)
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kClass;
    RegisterClassExW(&windowClass);
    // Like the island's render layer: drawn by Composition alone, clicks pass through.
    window_ = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                              kClass, L"Cup Island Flow", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!window_) {
        logLine("failed to create the floating cards' flow layer (0x%08lx)", GetLastError());
        return false;
    }
    target_ = createWindowTarget(compositor, window_);
    root_ = compositor.CreateContainerVisual();
    target_.Root(root_);
    blob_.build(compositor, device, root_, {}, {});
    auto queue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    end_ = queue.CreateTimer();
    end_.IsRepeating(false);
    end_.Tick([this](auto&&, auto&&) { finish(); });
    hide_ = queue.CreateTimer();
    hide_.IsRepeating(false);
    hide_.Tick([this](auto&&, auto&&) {
        if (window_) ShowWindow(window_, SW_HIDE);
    });
    built_ = true;
    return true;
}

void FloatFlow::out(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, Rect pill, Rect card,
                    std::function<void()> done)
{
    play(instance, compositor, device, pill, card, true, std::move(done));
}

void FloatFlow::in(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, Rect card, Rect pill,
                   std::function<void()> done)
{
    play(instance, compositor, device, card, pill, false, std::move(done));
}

void FloatFlow::play(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, Rect from, Rect to, bool outward,
                     std::function<void()> done)
{
    if (done_) { // one at a time: the one running ends where it was going
        end_.Stop();
        finish();
    }
    const Tokens& t = tokens();
    if (t.reducedMotion || !make(instance, compositor, device)) {
        done();
        return;
    }
    done_ = std::move(done);
    // The layer over both, with room for the shadow: the shape moves inside it.
    Rect area = around(from, to, 2 * t.shadowBlur + std::abs(t.shadowOffset.y));
    hide_.Stop();
    SetWindowPos(window_, HWND_TOPMOST, px(area.x), px(area.y), px(area.width), px(area.height), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    root_.Size({ area.width, area.height });
    Vec2 start{ from.x - area.x, from.y - area.y }, end{ to.x - area.x, to.y - area.y };

    // Where it starts, now: the pill's shape on the edge, or the card's.
    blob_.snapTo({ from.width, from.height }, start);
    blob_.setShapeNow(outward, outward ? t.maxCornerRadius : t.floatCornerRadius);
    // ...then off the edge (or onto it) along the spring a morph that size takes:
    // growing overshoots a little, shrinking doesn't (decision 080).
    Motion motion = sceneMotion(sceneAt(from), sceneAt(to), {}, t).morph;
    blob_.place({ to.width, to.height }, end, motion);
    blob_.flowShape(!outward, outward ? t.floatCornerRadius : t.maxCornerRadius, motion.duration);
    end_.Interval(std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(motion.duration));
    end_.Start();
}

void FloatFlow::finish()
{
    if (auto done = std::exchange(done_, nullptr)) done();
    if (hide_) {
        // The card's window draws under the layer before it goes: nothing blinks (decision 008's hand-offs).
        hide_.Interval(std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(tokens().floatHandOff));
        hide_.Start();
    }
}

void FloatFlow::destroy()
{
    if (end_) end_.Stop();
    if (hide_) hide_.Stop();
    end_ = hide_ = nullptr;
    done_ = nullptr;
    target_ = nullptr;
    root_ = nullptr;
    if (window_) DestroyWindow(std::exchange(window_, nullptr));
    built_ = false;
}

} // namespace cup::platform
