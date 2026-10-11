// Cup Island entry point: wiring only. Owns the IslandState, turns window
// input into core Events, and pushes the resulting Scene to the renderer.
// The loop: input -> reduce() -> layout() -> renderer + interaction layer.
// Tuning: edit assets/tokens.json while it runs; saving applies it live.
//
// Demo keys (click the pill first so it has keyboard focus):
//   M  toggle the demo music player        T  toggle a second activity (Split)
//   C  post a critical alert               D  toggle a simulated accepted drag
//   H  toggle hidden                       Esc collapse when expanded, otherwise quit

#include "app/live_tokens.h"
#include "core/hit_test.h"
#include "core/island.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "platform/file_watcher.h"
#include "platform/interaction_layer.h"
#include "platform/pill_renderer.h"
#include "platform/render_layer.h"
#include "providers/demo/demo_media_provider.h"
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
    if (g_renderer.apply(next)) followMorph(next.size, next.offset);
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
    Content content{ .label = "Timer", .compactLeading = { row({ Icon{ Glyph::Warning } }) },
                     .compactTrailing = { row({ Text{ "4:59", TextStyle::Digits } }) },
                     .expandedCenter = { row({ Text{ "Timer", TextStyle::Title } }), row({ Text{ "Demo activity" } }) } };
    dispatch(ActivityStarted{ { .id = id, .content = content } }, "demo key");
}

// Mouse-down: a button gets its own press feedback, anything else squashes the pill.
void onPressDown(Vec2 point)
{
    auto hit = hitTest(g_scene, point);
    g_pressedKey = hit ? hit->key : "";
    if (hit) g_renderer.pressElement(g_pressedKey);
    else g_renderer.pressDown();
}

void onPressRelease()
{
    if (!g_pressedKey.empty()) g_renderer.releaseElement(g_pressedKey);
    else g_renderer.pressRelease();
}

void onClick(Vec2 point)
{
    if (auto hit = hitTest(g_scene, point)) {
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
    } else if (id == TimerId::Demo) {
        // Unattended intro so there's something to see without input.
        g_providers->start("demo");
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
    callbacks.onPressRelease = onPressRelease;
    callbacks.onClick = onClick;
    callbacks.onClickOutside = [] { dispatch(ClickedOutside{}, "click outside"); };
    callbacks.onPointerMoved = [] { dispatch(PointerMoved{}, "pointer"); };
    callbacks.onKeyDown = onKey;
    callbacks.onTimer = onTimer;

    g_inputHwnd = createInteractionLayer(instance, render.left, g_scene.size, g_scene.offset, callbacks);
    if (!g_inputHwnd) {
        std::printf("Failed to create interaction window (0x%08lx)\n", GetLastError());
        return 1;
    }

    g_providers = std::make_unique<ProviderRegistry>(dispatch, now);
    g_providers->add(std::make_unique<DemoMediaProvider>());

    // Debounced: one save fires several notifications; reload once it settles.
    watchDirectoryOf(g_tokensFile, [] { setTimerFromAnyThread(TimerId::TokensReload, Ms{ 50 }); });

    ShowWindow(render.hwnd, SW_SHOWNOACTIVATE);
    ShowWindow(g_inputHwnd, SW_SHOW);
    setTimer(TimerId::Demo, Ms{ 700 });

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
