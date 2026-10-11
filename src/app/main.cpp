// Cup Island entry point: wiring only. Owns the IslandState, turns window
// input into core Events, and pushes the resulting Scene to the renderer.
// The loop: input -> reduce() -> layout() -> renderer + interaction layer.
// Tuning: edit assets/tokens.json while it runs; saving applies it live.
//
// Demo keys (click the pill first so it has keyboard focus):
//   M  toggle the demo music player (real media shows by itself)
//   T  toggle a second activity (Split). Click the bubble to open it, or drag
//      it onto the pill to make it the main one.
//   C  post a critical alert               D  toggle a simulated accepted drag
//   H  toggle hidden                       Esc collapse when expanded, otherwise quit

#include "app/live_tokens.h"
#include "core/hit_test.h"
#include "core/island.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "platform/file_watcher.h"
#include "platform/fullscreen_watcher.h"
#include "platform/interaction_layer.h"
#include "platform/pill_renderer.h"
#include "platform/render_layer.h"
#include "providers/demo/demo_media_provider.h"
#include "providers/media/smtc_media_provider.h"
#include "providers/provider_registry.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>

using namespace cup;
using namespace cup::platform;

namespace {

const auto g_startTime = std::chrono::steady_clock::now();
IslandState g_state;
Scene g_scene;
PillRenderer g_renderer;
HWND g_inputHwnd{};
std::filesystem::path g_tokensFile;
std::unique_ptr<ProviderRegistry> g_providers;
std::string g_pressedKey; // element under the mouse-down, if it was a button

// What the current (or last) mouse-down landed on. A press on the bubble
// becomes a drag once core says it moved far enough (BubbleDragged).
enum class Pressed { Nothing, Pill, Bubble };
Pressed g_pressed = Pressed::Nothing;
Vec2 g_pressStart;
bool g_dragged = false; // this press turned into a drag: its release is not a click

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

std::optional<Ms> earliest(std::optional<Ms> a, std::optional<Ms> b)
{
    if (!a) return b;
    if (!b) return a;
    return std::min(*a, *b);
}

void dispatch(const Event& event, const char* reason)
{
    Ms t = now();
    g_state = reduce(g_state, event, t);
    Scene next = layout(g_state, t);

    if (next.mode != g_scene.mode) {
        std::printf("[cup-island] %s -> %s (%s) %s\n", toString(g_scene.mode), toString(next.mode), reason, next.label.c_str());
        std::fflush(stdout);
    }
    g_renderer.apply(next);
    Rect bounds = interactiveBounds(next);
    if (bounds != interactiveBounds(g_scene)) followMorph({ bounds.width, bounds.height }, { bounds.x, bounds.y });
    g_scene = next;

    // One timer for everything time-based: core deadlines and ticking labels.
    auto deadline = earliest(nextDeadline(g_state), sceneDeadline(g_scene));
    setTimer(TimerId::Deadline, deadline ? std::optional<Ms>(std::max(*deadline - t, Ms{ 0 })) : std::nullopt);
}

void toggleProvider(const char* id)
{
    if (g_providers->isRunning(id)) g_providers->stop(id);
    else g_providers->start(id);
}

void toggleTimer()
{
    const std::string id = "demo.timer";
    if (hasActivity(id)) {
        dispatch(ActivityEnded{ id }, "demo key");
        return;
    }
    Content content{ .label = "Timer", .compactLeading = { row({ Icon{ Glyph::Timer } }) },
                     .compactTrailing = { row({ Text{ "4:59", TextStyle::Digits } }) },
                     .minimal = { row({ Icon{ Glyph::Timer } }) },
                     .expandedCenter = { row({ Text{ "Timer", TextStyle::Title } }), row({ Text{ "Demo activity" } }) } };
    dispatch(ActivityStarted{ { .id = id, .content = content } }, "demo key");
}

Vec2 toPill(Vec2 point) { return { point.x - g_scene.offset.x, point.y - g_scene.offset.y }; }
bool onBubble(Vec2 point) { return g_scene.bubble && g_scene.bubble->frame.contains(point); }

// Mouse-down: a button gets its own press feedback, anything else squashes
// the pill, or the bubble.
void onPressDown(Vec2 point)
{
    g_dragged = false;
    g_pressedKey.clear();
    if (onBubble(point)) {
        g_pressed = Pressed::Bubble;
        g_pressStart = point;
        g_renderer.pressDown(true);
        return;
    }
    g_pressed = Pressed::Pill;
    auto hit = hitTest(g_scene, toPill(point));
    if (hit) g_pressedKey = hit->key;
    if (hit) g_renderer.pressElement(g_pressedKey);
    else g_renderer.pressDown(false);
}

void onPressMoved(Vec2 point)
{
    if (g_pressed != Pressed::Bubble) return;
    dispatch(BubbleDragged{ { point.x - g_pressStart.x, point.y - g_pressStart.y } }, "drag");
    g_dragged = g_dragged || g_state.bubbleDrag.has_value(); // the renderer lifts it from here
}

void onPressRelease(std::optional<Vec2> point)
{
    if (g_pressed == Pressed::Bubble) {
        if (!g_dragged) g_renderer.pressRelease(true);
        if (g_state.bubbleDrag) dispatch(point ? Event{ BubbleReleased{} } : Event{ BubbleDragCancelled{} }, "drop");
    } else if (!g_pressedKey.empty()) {
        g_renderer.releaseElement(g_pressedKey);
    } else {
        g_renderer.pressRelease(false);
    }
}

void onClick(Vec2 point)
{
    if (g_dragged) return;
    if (g_pressed == Pressed::Bubble) {
        dispatch(BubbleClicked{}, "bubble click");
        return;
    }
    Rect pill{ g_scene.offset.x, g_scene.offset.y, g_scene.size.x, g_scene.size.y };
    if (!pill.contains(point)) return; // the gap between pill and bubble
    if (auto hit = hitTest(g_scene, toPill(point))) {
        if (auto event = islandEvent(hit->action)) {
            dispatch(*event, "switcher");
            return;
        }
        g_providers->route(hit->action);
        dispatch(PointerMoved{}, "action"); // the user is busy here: keep Expanded open
    } else {
        dispatch(PillClicked{}, "click");
    }
}

void onKey(WPARAM key)
{
    switch (key) {
    case VK_ESCAPE:
        if (mode(g_state) == Mode::Expanded) dispatch(EscapePressed{}, "esc");
        else DestroyWindow(g_inputHwnd);
        break;
    case 'M': toggleProvider("demo"); break;
    case 'T': toggleTimer(); break;
    case 'C': {
        Content content{ .label = "Battery 5%", .alertLeading = { row({ Icon{ Glyph::Warning } }) },
                         .alertCenter = { row({ Text{ "Battery 5%", TextStyle::Title } }), row({ Text{ "Plug in soon" } }) } };
        dispatch(AlertPosted{ { .level = Level::Critical, .content = content } }, "demo key");
        break;
    }
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
    } else if (id == TimerId::TokensReload) {
        if (app::loadTokens(g_tokensFile, /* startup */ false)) {
            g_renderer.refreshAnimations();
            dispatch(Tick{}, "tokens.json"); // re-layout with the new sizes
        }
    }
}

} // namespace

int main()
{
    std::printf("Cup Island prototype\n");
    std::printf("Click the pill to expand/collapse. Keys: M T C D H, Esc to quit.\n\n");
    std::fflush(stdout);

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    auto queueController = createDispatcherQueueForCurrentThread();

    g_tokensFile = app::findTokensFile();
    app::loadTokens(g_tokensFile, /* startup */ true);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    RenderLayer render = createRenderLayer(instance, tokens().canvas);
    if (!render.hwnd) {
        std::printf("Failed to create render window (0x%08lx)\n", GetLastError());
        return 1;
    }

    g_scene = layout(g_state, now());
    g_renderer.build(render.compositor, render.root, g_scene);

    InteractionCallbacks callbacks;
    callbacks.onPressDown = onPressDown;
    callbacks.onPressMoved = onPressMoved;
    callbacks.onPressRelease = onPressRelease;
    callbacks.onClick = onClick;
    callbacks.onClickOutside = [] { dispatch(ClickedOutside{}, "click outside"); };
    callbacks.onPointerMoved = [] { dispatch(PointerMoved{}, "pointer"); };
    callbacks.onKeyDown = onKey;
    callbacks.onTimer = onTimer;

    Rect bounds = interactiveBounds(g_scene);
    g_inputHwnd = createInteractionLayer(instance, render.left, { bounds.width, bounds.height }, { bounds.x, bounds.y }, callbacks);
    if (!g_inputHwnd) {
        std::printf("Failed to create interaction window (0x%08lx)\n", GetLastError());
        return 1;
    }

    // WinRT events arrive on background threads; providers hop to this (UI) thread with it.
    auto uiQueue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    auto postToUi = [uiQueue](std::function<void()> work) { uiQueue.TryEnqueue([work = std::move(work)] { work(); }); };

    g_providers = std::make_unique<ProviderRegistry>(dispatch, now);
    g_providers->add(std::make_unique<SmtcMediaProvider>(postToUi));
    g_providers->add(std::make_unique<DemoMediaProvider>()); // M key
    g_providers->start("media");

    // TR §9.2: hide over borderless/exclusive fullscreen apps on this monitor.
    watchFullscreen(render.hwnd, [](bool fullscreen) { dispatch(HiddenChanged{ fullscreen }, "fullscreen"); });

    // Debounced: one save fires several notifications; reload once it settles.
    watchDirectoryOf(g_tokensFile, [] { setTimerFromAnyThread(TimerId::TokensReload, Ms{ 50 }); });

    ShowWindow(render.hwnd, SW_SHOWNOACTIVATE);
    ShowWindow(g_inputHwnd, SW_SHOW);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_providers.reset(); // stops providers while the window still exists
    DestroyWindow(render.hwnd);
    queueController.ShutdownQueueAsync().get();
    return static_cast<int>(msg.wParam);
}
