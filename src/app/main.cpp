// Cup Island entry point: wiring only. Owns the IslandState, turns window
// input into core Events, and pushes the resulting Scene to the renderer.
// The loop: input -> reduce() -> layout() -> renderer + interaction layer.
//
// Demo keys (click the pill first so it has keyboard focus):
//   M  toggle the fake media activity      T  toggle a second activity (Split)
//   A  post a track-change alert           C  post a critical alert
//   D  toggle a simulated accepted drag    H  toggle hidden
//   Esc collapse when expanded, otherwise quit

#include "core/island.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "platform/interaction_layer.h"
#include "platform/pill_renderer.h"
#include "platform/render_layer.h"

#include <chrono>
#include <cstdio>

using namespace cup;
using namespace cup::platform;

namespace {

const auto g_startTime = std::chrono::steady_clock::now();
IslandState g_state;
Scene g_scene;
PillRenderer g_renderer;
HWND g_inputHwnd{};

Ms now()
{
    return std::chrono::duration_cast<Ms>(std::chrono::steady_clock::now() - g_startTime);
}

bool hasActivity(const std::string& id)
{
    for (const auto& a : g_state.activities)
        if (a.id == id) return true;
    return false;
}

void dispatch(const Event& event, const char* reason)
{
    Ms t = now();
    g_state = reduce(g_state, event, t);
    Scene next = layout(g_state);

    if (next.mode != g_scene.mode) {
        std::printf("[cup-island] %s -> %s (%s)\n", toString(g_scene.mode), toString(next.mode), reason);
        std::fflush(stdout);
    }
    // Only touch visuals on a real change: re-setting a property restarts its
    // implicit animation, which would stall a morph in flight.
    bool geometryChanged = next.size != g_scene.size || next.offset != g_scene.offset;
    if (geometryChanged || next.detailOpacity != g_scene.detailOpacity) g_renderer.apply(next);
    if (geometryChanged) followMorph(next.size, next.offset);
    g_scene = next;

    auto deadline = nextDeadline(g_state);
    setTimer(TimerId::Deadline, deadline ? std::optional<Ms>(*deadline - t) : std::nullopt);
}

void toggleActivity(const std::string& id, const char* title, const char* subtitle)
{
    if (hasActivity(id)) dispatch(ActivityEnded{ id }, "demo key");
    else dispatch(ActivityStarted{ { .id = id, .content = { title, subtitle } } }, "demo key");
}

void onKey(WPARAM key)
{
    switch (key) {
    case VK_ESCAPE:
        if (mode(g_state) == Mode::Expanded) dispatch(EscapePressed{}, "esc");
        else DestroyWindow(g_inputHwnd);
        break;
    case 'M': toggleActivity("media", "Song title", "Artist"); break;
    case 'T': toggleActivity("timer", "Timer", "04:59"); break;
    case 'A':
        dispatch(AlertPosted{ { .activityId = "media", .content = { "Next song", "Artist" }, .coalesceKey = "track" } },
                 "demo key");
        break;
    case 'C':
        dispatch(AlertPosted{ { .level = Level::Critical, .content = { "Battery 5%", "" } } }, "demo key");
        break;
    case 'D':
        if (g_state.dragging) dispatch(DragLeft{}, "demo key");
        else dispatch(DragEntered{ true }, "demo key");
        break;
    case 'H': dispatch(HiddenChanged{ !g_state.hidden }, "demo key"); break;
    }
}

void onTimer(TimerId id)
{
    if (id == TimerId::Deadline) {
        dispatch(Tick{}, "timer");
    } else if (id == TimerId::Demo) {
        // Unattended intro so the morphs are visible without input: a media
        // activity appears with a track alert, then settles into Compact.
        dispatch(ActivityStarted{ { .id = "media", .content = { "Song title", "Artist" } } }, "demo");
        dispatch(AlertPosted{ { .activityId = "media", .content = { "Now playing", "Song title" }, .coalesceKey = "track" } },
                 "demo");
    }
}

} // namespace

int main()
{
    std::printf("Cup Island prototype\n");
    std::printf("Click the pill to expand/collapse. Keys: M T A C D H, Esc to quit.\n\n");
    std::fflush(stdout);

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    auto queueController = createDispatcherQueueForCurrentThread();

    HINSTANCE instance = GetModuleHandleW(nullptr);
    RenderLayer render = createRenderLayer(instance, tokens::kCanvas);
    if (!render.hwnd) {
        std::printf("Failed to create render window (0x%08lx)\n", GetLastError());
        return 1;
    }

    g_scene = layout(g_state);
    g_renderer.build(render.compositor, render.root, g_scene);

    InteractionCallbacks callbacks;
    callbacks.onPressDown = [] { g_renderer.pressDown(); };
    callbacks.onPressRelease = [] { g_renderer.pressRelease(); };
    callbacks.onClick = [] { dispatch(PillClicked{}, "click"); };
    callbacks.onClickOutside = [] { dispatch(ClickedOutside{}, "click outside"); };
    callbacks.onPointerMoved = [] { dispatch(PointerMoved{}, "pointer"); };
    callbacks.onKeyDown = onKey;
    callbacks.onTimer = onTimer;

    g_inputHwnd = createInteractionLayer(instance, render.left, g_scene.size, g_scene.offset, callbacks);
    if (!g_inputHwnd) {
        std::printf("Failed to create interaction window (0x%08lx)\n", GetLastError());
        return 1;
    }

    ShowWindow(render.hwnd, SW_SHOWNOACTIVATE);
    ShowWindow(g_inputHwnd, SW_SHOW);
    setTimer(TimerId::Demo, Ms{ 700 });

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DestroyWindow(render.hwnd);
    queueController.ShutdownQueueAsync().get();
    return static_cast<int>(msg.wParam);
}
