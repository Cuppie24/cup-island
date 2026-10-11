// Cup Island entry point: wiring only. Owns the IslandState, turns window
// input into core Events, and pushes the resulting Scene to the renderer.
// The loop: input -> reduce() -> layout() -> renderer + interaction layer.
// Tuning: edit assets/tokens.json while it runs; saving applies it live.
//
// The idle pill shows the time; clicking it opens Home (decision 009): the
// clock, battery, theme and Bluetooth widgets, and the Pomodoro tile.
//
// Demo keys (click the pill first so it has keyboard focus):
//   M  toggle the demo music player (real media shows by itself)
//   T  toggle a second activity (Split). Click the bubble to open it, or drag
//      it onto the pill to make it the main one.
//   C  post a critical alert               D  toggle a simulated accepted drag
//   H  toggle hidden                       Esc collapse when expanded, otherwise quit
// Quit any time from the tray icon's right-click menu. Release builds have no
// console window (CMakeLists.txt), so the printf logging shows in Debug builds only.

#include "app/live_tokens.h"
#include "core/hit_test.h"
#include "core/island.h"
#include "core/layout.h"
#include "core/tabs.h"
#include "core/tokens.h"
#include "platform/file_watcher.h"
#include "platform/wall_clock.h"
#include "platform/fullscreen_watcher.h"
#include "platform/interaction_layer.h"
#include "platform/pill_renderer.h"
#include "platform/render_layer.h"
#include "platform/tray_icon.h"
#include "providers/demo/demo_media_provider.h"
#include "providers/media/smtc_media_provider.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/system/battery_provider.h"
#include "providers/system/bluetooth_provider.h"
#include "providers/system/theme_provider.h"
#include "providers/provider_registry.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

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

// The pointer over the pill. Feedback is the renderer's; what a click means is
// core's (hitTest). Content reacts to hover, the pill never does (decision 002).
struct Pointer {
    std::optional<Vec2> at;      // over the pill, pill coordinates
    std::string pressedKey;      // the button under the mouse-down
    std::optional<Placed> scrub; // the progress bar being dragged
    float scrubFraction = 0;
    std::optional<Vec2> bubblePress; // mouse-down on the Split bubble, render-layer coordinates
    bool dragged = false;            // ...and core took it as a drag (BubbleDragged): not a click
    std::optional<std::string> tabPress;  // mouse-down on an Expanded tab: its activity ("" = Home)
    std::optional<std::string> tilePress; // mouse-down on a Home tile, off its buttons: its panel
} g_pointer;

// Pointer callbacks get render-layer points; content works in pill coordinates.
Vec2 toPill(Vec2 point) { return { point.x - g_scene.offset.x, point.y - g_scene.offset.y }; }
bool onPill(Vec2 point) { return Rect{ g_scene.offset.x, g_scene.offset.y, g_scene.size.x, g_scene.size.y }.contains(point); }
bool onBubble(Vec2 point) { return g_scene.bubble && g_scene.bubble->frame.contains(point); }

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

const Placed* findElement(const std::string& key)
{
    for (const auto& p : g_scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

// The tab of what's shown (already marked): clicking it does nothing.
bool isCurrent(const Tab& tab)
{
    const auto& strip = *g_scene.tabs;
    return strip.active && &tab == &strip.tabs[*strip.active];
}

// Re-run on every pointer move and every new scene: content can move under a still pointer.
void updateHover()
{
    std::string key;
    if (g_pointer.scrub) {
        key = g_pointer.scrub->key; // stays lit while dragged, even off the bar
    } else if (g_pointer.at) {
        if (auto hit = hitTest(g_scene, *g_pointer.at)) key = hit->key;
    }
    g_renderer.setHovered(key);

    // Tabs light up as a whole, except the active one, which is already marked (decision 007).
    // Home's tiles too, unless the pointer is on one of their buttons (decision 009).
    const Tab* tab = g_scene.tabs && g_pointer.at ? tabAt(*g_scene.tabs, *g_pointer.at) : nullptr;
    if (tab && isCurrent(*tab)) tab = nullptr;
    const Tile* tile = g_pointer.at && key.empty() ? tileAt(g_scene, *g_pointer.at) : nullptr;
    if (tile && !tile->clickable()) tile = nullptr; // widgets without a card or tap action don't react
    if (tab) g_renderer.setAreaHovered(tab->frame, g_scene.tabs->radius);
    else if (tile) g_renderer.setAreaHovered(tile->frame, g_scene.tileRadius);
    else g_renderer.setAreaHovered(std::nullopt, 0);
}

// The Expanded tab under a pill-coordinate point, if any.
const Tab* tabUnder(Vec2 point) { return g_scene.tabs ? tabAt(*g_scene.tabs, point) : nullptr; }

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
    Scene next = layout(g_state, t, [](const Text& text) { return g_renderer.measure(text); });

    if (next.mode != g_scene.mode) {
        std::printf("[cup-island] %s -> %s (%s) %s\n", toString(g_scene.mode), toString(next.mode), reason, next.label.c_str());
        std::fflush(stdout);
    }
    g_renderer.apply(next);
    Rect bounds = interactiveBounds(next);
    if (bounds != interactiveBounds(g_scene)) followMorph({ bounds.width, bounds.height }, { bounds.x, bounds.y });
    g_scene = next;
    updateHover();

    // One timer for everything time-based: core deadlines, ticking labels and provider wake-ups.
    auto deadline = earliest(nextDeadline(g_state), sceneDeadline(g_scene));
    if (g_providers) deadline = earliest(deadline, g_providers->nextWake());
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

// Mouse-down: the bubble may become a drag, a progress bar starts scrubbing,
// a button gets its own press feedback, anything else squashes the pill.
void onPressDown(Vec2 canvasPoint)
{
    if (onBubble(canvasPoint)) {
        g_pointer.bubblePress = canvasPoint;
        g_pointer.dragged = false;
        g_renderer.pressDown(true);
        return;
    }
    Vec2 point = toPill(canvasPoint);
    g_pointer.at = point;
    if (const Tab* tab = tabUnder(point)) {
        g_pointer.tabPress = tab->id; // switches on release, if still on the same tab
        return;
    }
    auto hit = hitTest(g_scene, point);
    const Placed* element = hit ? findElement(hit->key) : nullptr;
    if (const Tile* tile = element ? nullptr : tileAt(g_scene, point)) {
        // Opens its card on release, if still on the same tile. A widget with no
        // card takes the click too, so it doesn't fall through and close Home.
        g_pointer.tilePress = tile->id;
        return;
    }
    if (element && std::holds_alternative<Progress>(element->item)) {
        g_pointer.scrub = *element;
        g_pointer.scrubFraction = hit->action.value;
        g_renderer.scrubElement(hit->key, g_pointer.scrubFraction);
    } else if (element) {
        g_pointer.pressedKey = hit->key;
        g_renderer.pressElement(hit->key);
    } else {
        g_renderer.pressDown(false);
    }
    updateHover();
}

void onPointerMoved(Vec2 canvasPoint, bool inside)
{
    if (g_pointer.bubblePress) {
        Vec2 start = *g_pointer.bubblePress;
        dispatch(BubbleDragged{ { canvasPoint.x - start.x, canvasPoint.y - start.y } }, "drag");
        if (g_state.bubbleDrag && !g_pointer.dragged) {
            g_pointer.dragged = true;
            g_renderer.pressRelease(true); // un-squash: it's being carried now
        }
        return;
    }
    Vec2 point = toPill(canvasPoint);
    g_pointer.at = inside && onPill(canvasPoint) ? std::optional<Vec2>(point) : std::nullopt;
    if (g_pointer.scrub) {
        g_pointer.scrubFraction = seekFraction(*g_pointer.scrub, point.x);
        g_renderer.scrubElement(g_pointer.scrub->key, g_pointer.scrubFraction);
    }
    dispatch(PointerMoved{}, "pointer"); // keeps Expanded open while in use; refreshes hover
}

void onPressRelease(Vec2 canvasPoint, Release how)
{
    if (g_pointer.tabPress) {
        std::string id = *std::exchange(g_pointer.tabPress, std::nullopt);
        const Tab* tab = how == Release::Inside && onPill(canvasPoint) ? tabUnder(toPill(canvasPoint)) : nullptr;
        if (tab && tab->id == id && !isCurrent(*tab)) dispatch(id.empty() ? Event{ HomeOpened{} } : Event{ ActivitySwitched{ id } }, "tab");
        return;
    }
    if (g_pointer.tilePress) {
        std::string id = *std::exchange(g_pointer.tilePress, std::nullopt);
        const Tile* tile = how == Release::Inside && onPill(canvasPoint) ? tileAt(g_scene, toPill(canvasPoint)) : nullptr;
        if (!tile || tile->id != id) return;
        if (!tile->tapAction.empty()) {
            g_providers->route({ tile->providerId, tile->id, tile->tapAction, 0 }); // a toggle: no card
            dispatch(PointerMoved{}, "tile action");                                  // the user is busy here: keep Home open
        } else if (tile->opens) {
            dispatch(TileClicked{ id }, "tile");
        }
        return;
    }
    if (g_pointer.bubblePress) {
        g_pointer.bubblePress.reset();
        if (!g_pointer.dragged) g_renderer.pressRelease(true);
        if (g_state.bubbleDrag) dispatch(how == Release::Cancelled ? Event{ BubbleDragCancelled{} } : Event{ BubbleReleased{} }, "drop");
        else if (!g_pointer.dragged && how == Release::Inside && onBubble(canvasPoint)) dispatch(BubbleClicked{}, "bubble click");
        return;
    }
    Vec2 point = toPill(canvasPoint);
    if (g_pointer.scrub) {
        // A scrub seeks wherever it's let go, even off the pill; only a lost capture cancels it.
        Placed bar = *std::exchange(g_pointer.scrub, std::nullopt);
        bool commit = how != Release::Cancelled;
        g_renderer.endScrub(bar.key, commit ? std::optional<float>(g_pointer.scrubFraction) : std::nullopt, now());
        if (commit) g_providers->route({ bar.providerId, bar.activityId, "seek", g_pointer.scrubFraction });
        dispatch(PointerMoved{}, "seek");
        return;
    }
    if (!g_pointer.pressedKey.empty()) {
        std::string key = std::exchange(g_pointer.pressedKey, std::string{});
        g_renderer.releaseElement(key);
        // Only a release on the same button counts: sliding off cancels, like any button.
        auto hit = how == Release::Inside && onPill(canvasPoint) ? hitTest(g_scene, point) : std::nullopt;
        if (hit && hit->key == key) {
            g_providers->route(hit->action);
            dispatch(PointerMoved{}, "action"); // the user is busy here: keep Expanded open
        }
        return;
    }
    g_renderer.pressRelease(false);
    if (how == Release::Inside && onPill(canvasPoint) && !hitTest(g_scene, point)) dispatch(PillClicked{}, "click");
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
        g_providers->wakeDue(now());
        // Re-read the wall clock every time: it also does Tick's work, and a
        // clock or time-zone change shows within a minute (decision 009).
        dispatch(readWallClock(), "timer");
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
    callbacks.onPressRelease = onPressRelease;
    callbacks.onClickOutside = [] { dispatch(ClickedOutside{}, "click outside"); };
    callbacks.onPointerMoved = onPointerMoved;
    callbacks.onPointerLeft = [] {
        g_pointer.at.reset();
        updateHover();
    };
    callbacks.onKeyDown = onKey;
    callbacks.onTimer = onTimer;

    Rect bounds = interactiveBounds(g_scene);
    g_inputHwnd = createInteractionLayer(instance, render.left, { bounds.width, bounds.height }, { bounds.x, bounds.y }, callbacks);
    if (!g_inputHwnd) {
        std::printf("Failed to create interaction window (0x%08lx)\n", GetLastError());
        return 1;
    }
    createTrayIcon(instance, [] { DestroyWindow(g_inputHwnd); }); // right-click -> Exit, same as Esc

    // WinRT events arrive on background threads; providers hop to this (UI) thread with it.
    auto uiQueue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    auto postToUi = [uiQueue](std::function<void()> work) { uiQueue.TryEnqueue([work = std::move(work)] { work(); }); };

    g_providers = std::make_unique<ProviderRegistry>(dispatch, now);
    g_providers->add(std::make_unique<SmtcMediaProvider>(postToUi));
    g_providers->add(std::make_unique<DemoMediaProvider>()); // M key
    // Home's tiles follow the order panels first appear: widgets, then Pomodoro.
    g_providers->add(std::make_unique<BatteryProvider>(postToUi));
    g_providers->add(std::make_unique<BluetoothProvider>(postToUi));
    g_providers->add(std::make_unique<ThemeProvider>(postToUi));
    g_providers->add(std::make_unique<PomodoroProvider>());
    g_providers->start("media");
    g_providers->start("system.battery");
    g_providers->start("system.theme");
    g_providers->start("system.bluetooth");
    g_providers->start("pomodoro");
    dispatch(readWallClock(), "clock");

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

    removeTrayIcon();
    g_providers.reset(); // stops providers while the window still exists
    DestroyWindow(render.hwnd);
    queueController.ShutdownQueueAsync().get();
    return static_cast<int>(msg.wParam);
}
