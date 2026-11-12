#include "app/provider_windows.h"

#include "core/tokens.h"
#include "core/tokens_json.h"
#include "core/window_keys.h"
#include "core/window_layout.h"
#include "core/window_placement.h"
#include "platform/displays.h"
#include "platform/log.h"
#include "platform/provider_window.h"
#include "platform/stay_on_top.h"
#include "providers/provider_registry.h"

#include <exception>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace cup::app {
namespace {

struct Shown {
    WindowKey key;
    std::unique_ptr<platform::ProviderWindow> window;
    std::uint64_t raised = 0; // OpenWindow::raised last acted on
    std::string page;         // the page laid out: a new one starts at its top
    float scroll = 0;
    WindowScene scene;
    std::optional<Vec2> pointer;
    std::optional<WindowHit> pressed; // what the button went down on
    std::string focus;                // the keyboard's stop (window_keys.h): "" until a key is pressed, and after a click
    bool keys = false;                // the keyboard is in use: a stop that goes (a page opened by Enter) hands focus to the page's first
};

WindowHooks g_hooks;
std::map<WindowKey, std::unique_ptr<Shown>> g_shown;
Places g_places;
bool g_placesRead = false;
bool g_syncQueued = false;
std::optional<Tokens> g_drawnFor; // what the windows were drawn for: tells a recolour from a resize
bool g_glass = false;              // windows as glass (setWindowsGlass)

// A queued call's work, its errors logged: one escaping a DispatcherQueue
// callback is a stowed exception (0xC000027B) and ends the app.
void guarded(const char* what, const std::function<void()>& work)
{
    try {
        work();
    } catch (winrt::hresult_error const& e) {
        platform::logLine("WinRT error %s: 0x%08x %ls", what, static_cast<unsigned int>(e.code()), e.message().c_str());
    } catch (std::exception const& e) {
        platform::logLine("Exception %s: %s", what, e.what());
    }
}

std::string placeKey(const WindowKey& key) { return key.providerId + "/" + key.windowId; }

// Every monitor as a Screen. Tokens are already in the island's display's
// pixels (decision 017), so a screen's scale is 1 here; per-monitor scale is the island's to do first.
std::vector<Screen> screens()
{
    std::vector<Screen> out;
    for (const platform::Display& d : platform::listDisplays()) out.push_back({ d.id, d.workArea, 1.0f });
    return out;
}

const Places& places()
{
    if (!g_placesRead) {
        std::ifstream in(g_hooks.placesFile, std::ios::binary);
        std::stringstream text;
        text << in.rdbuf();
        g_places = parsePlaces(text.str());
        g_placesRead = true;
    }
    return g_places;
}

void rememberPlace(const Shown& s)
{
    Rect at = s.window->frame();
    auto all = screens();
    if (all.empty() || at.width <= 0) return;
    Vec2 middle{ at.x + at.width / 2, at.y + at.height / 2 };
    const Screen* on = &all.front();
    for (const Screen& screen : all)
        if (screen.workArea.contains(middle)) on = &screen;
    places();
    remember(g_places, placeKey(s.key), s.key.providerId, savedPlace(at, *on));
    if (g_hooks.placesFile.empty()) return;
    std::ofstream out(g_hooks.placesFile, std::ios::binary | std::ios::trunc);
    out << formatPlaces(g_places);
}

// The focus ring where the keyboard's stop is (decision 033), or none: no
// stop, it went (its row or page changed), or a page's item scrolled out of
// the pane by the wheel, where a ring would cover the title bar.
void showFocus(Shown& s)
{
    auto f = findWindowFocus(s.scene, s.focus);
    if (!f && s.keys) { // its page changed under it (Enter opened a page, Esc went back): the new page's first stop
        f = pageEndFocus(s.scene, false);
        if (f) s.focus = f->id;
    }
    if (!f) s.focus.clear();
    bool shows = f && (!f->inPage || (f->frame.y >= s.scene.pane.y - 0.5f && f->frame.y + f->frame.height <= s.scene.pane.y + s.scene.pane.height + 0.5f));
    s.window->setFocusRing(shows ? std::optional<Rect>(f->frame) : std::nullopt, f ? f->radius : 0);
}

// Lays it out at its size now and draws it: after any change, and in the
// size message itself while it's resized, so the drawing keeps up.
void relayout(Shown& s)
{
    const OpenWindow* w = findWindow(g_hooks.registry->windows(), s.key);
    if (!w) return;
    if (w->page != s.page) { // another page: from its top
        s.page = w->page;
        s.scroll = 0;
    }
    Ms now = g_hooks.now();
    s.scene = windowLayout(*w, s.window->clientSize(), s.scroll, now, g_hooks.measure);
    s.scroll = s.scene.scroll;
    s.window->draw(s.scene, now);
    showFocus(s);
}

void showHover(Shown& s)
{
    std::string key;
    std::optional<std::size_t> nav;
    std::optional<Rect> row;
    float radius = 0;
    if (s.pointer) {
        WindowHit hit = windowHitTest(s.scene, *s.pointer);
        if (hit.hit) key = hit.hit->key;
        if (hit.part == WindowPart::Page)
            for (std::size_t i = 0; i < s.scene.nav.size(); ++i)
                if (s.scene.nav[i].page == hit.page) nav = i;
        for (const Placed& p : s.scene.page) // a clickable row lights as a whole, as on the island (decision 090)
            if (p.key == key && p.listRow) row = p.listRow, radius = p.listRowRadius;
    }
    s.window->setHovered(key, nav, row, radius);
}

Shown* find(const WindowKey& key)
{
    auto it = g_shown.find(key);
    return it == g_shown.end() ? nullptr : it->second.get();
}

void act(const Shown& s, const WindowHit& hit)
{
    ProviderRegistry& registry = *g_hooks.registry;
    switch (hit.part) {
    case WindowPart::Content: registry.route(hit.hit->action); break;
    case WindowPart::Page: registry.windowPageShown(s.key, hit.page); break;
    case WindowPart::Close: registry.windowClosed(s.key); break;
    case WindowPart::Minimize: s.window->minimize(); break;
    case WindowPart::Caption:
    case WindowPart::None: break;
    }
}

// The keyboard lands on `f`: scrolled into view, its ring drawn.
void focusOn(Shown& s, const std::optional<WindowFocus>& f)
{
    if (!f) return;
    s.keys = true;
    s.focus = f->id;
    s.scroll = scrollToShow(s.scene, *f);
    relayout(s);
    showHover(s);
}

// Keys in a window (decision 104), the island's own (decisions 031, 033):
// the arrows move between the page's stops and the sidebar's pages, Tab and
// Shift+Tab through all of them, Home and End to the page's first and last;
// Enter and Space click what has focus. Esc steps back from a page within a
// page, else closes it, as Ctrl+W does; Ctrl+Tab goes through its pages;
// Page Up and Page Down, Ctrl+Home and Ctrl+End scroll it.
void key(const WindowKey& id, WPARAM vk)
{
    if (g_hooks.takeKey && g_hooks.takeKey(id, vk)) return;
    Shown* s = find(id);
    if (!s) return;
    ProviderRegistry& registry = *g_hooks.registry;
    bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0, alt = GetKeyState(VK_MENU) < 0;
    if (alt) return; // Alt's keys are Windows' (Alt+Space, Alt+F4)
    s->keys = true;
    auto arrow = vk == VK_UP ? std::optional<Arrow>(Arrow::Up) : vk == VK_DOWN ? std::optional<Arrow>(Arrow::Down)
               : vk == VK_LEFT ? std::optional<Arrow>(Arrow::Left) : vk == VK_RIGHT ? std::optional<Arrow>(Arrow::Right) : std::nullopt;
    if (arrow && !ctrl) return focusOn(*s, moveWindowFocus(s->scene, s->focus, *arrow));
    if (vk == VK_TAB && !ctrl) return focusOn(*s, stepWindowFocus(s->scene, s->focus, shift));
    if ((vk == VK_HOME || vk == VK_END) && !ctrl) return focusOn(*s, pageEndFocus(s->scene, vk == VK_END));
    if ((vk == VK_RETURN || vk == VK_SPACE) && !ctrl) {
        auto f = findWindowFocus(s->scene, s->focus);
        if (!f) return focusOn(*s, moveWindowFocus(s->scene, "", Arrow::Down)); // nothing focused: show where it is first
        s->keys = true;
        act(*s, activateFocus(s->scene, *f));
        return;
    }
    if (vk == VK_ESCAPE && !s->scene.backAction.empty()) registry.route({ id.providerId, "", s->scene.backAction, 0, "", id.windowId });
    else if (vk == VK_ESCAPE || (ctrl && vk == 'W')) registry.windowClosed(id);
    else if (ctrl && vk == VK_TAB && !s->scene.nav.empty()) {
        std::size_t at = 0, n = s->scene.nav.size();
        for (std::size_t i = 0; i < n; ++i)
            if (s->scene.nav[i].picked) at = i;
        registry.windowPageShown(id, s->scene.nav[(at + (shift ? n - 1 : 1)) % n].page);
    } else if (vk == VK_NEXT || vk == VK_PRIOR || vk == VK_HOME || vk == VK_END) { // Home and End with Ctrl
        float page = s->scene.pane.height * 0.9f;
        s->scroll = vk == VK_HOME ? 0 : vk == VK_END ? s->scene.maxScroll : s->scroll + (vk == VK_NEXT ? page : -page);
        relayout(*s);
        showHover(*s);
    }
}

platform::ProviderWindowCallbacks callbacksFor(const WindowKey& id)
{
    platform::ProviderWindowCallbacks c;
    c.isCaption = [id](Vec2 p) {
        Shown* s = find(id);
        return s && windowHitTest(s->scene, p).part == WindowPart::Caption;
    };
    c.onPointerMoved = [id](Vec2 p) {
        if (Shown* s = find(id)) {
            s->pointer = p;
            showHover(*s);
        }
    };
    c.onPointerLeft = [id] {
        if (Shown* s = find(id)) {
            s->pointer.reset();
            showHover(*s);
        }
    };
    c.onPressDown = [id](Vec2 p) {
        Shown* s = find(id);
        if (!s) return;
        s->keys = false;
        if (!s->focus.empty()) { // the pointer's turn: the ring goes until a key is pressed again
            s->focus.clear();
            showFocus(*s);
        }
        WindowHit hit = windowHitTest(s->scene, p);
        if (hit.part == WindowPart::None || hit.part == WindowPart::Caption) return;
        s->pressed = hit;
        if (hit.hit) s->window->press(hit.hit->key);
    };
    c.onPressRelease = [id](Vec2 p, bool inside) {
        Shown* s = find(id);
        if (!s || !s->pressed) return;
        WindowHit was = *std::exchange(s->pressed, std::nullopt);
        if (was.hit) s->window->release(was.hit->key);
        if (!inside) return;
        // Only a release on what was pressed counts: sliding off cancels, like any button.
        WindowHit now = windowHitTest(s->scene, p);
        bool same = now.part == was.part && now.page == was.page && (!now.hit || (was.hit && now.hit->key == was.hit->key));
        if (same) act(*s, now); // the segment let go on, not the one pressed: its index is the release's
    };
    c.onWheel = [id](Vec2, float notches) {
        Shown* s = find(id);
        if (!s) return;
        s->scroll -= notches * 3 * tokens().smallButtonSize; // three small rows a notch
        relayout(*s);
        showHover(*s);
    };
    c.onKeyDown = [id](WPARAM vk) { key(id, vk); };
    c.onCloseRequested = [id] { g_hooks.registry->windowClosed(id); };
    c.onResized = [id](Vec2) {
        if (Shown* s = find(id)) {
            relayout(*s);
            showHover(*s);
        }
    };
    c.onMoved = [id] {
        if (Shown* s = find(id)) rememberPlace(*s);
    };
    return c;
}

void openWindowFor(const OpenWindow& w)
{
    if (!g_drawnFor) g_drawnFor = tokens();
    std::vector<Rect> taken;
    for (const auto& [key, s] : g_shown) taken.push_back(s->window->frame());
    Rect frame = placeWindow(w.spec, screens(), g_hooks.islandScreen(), g_hooks.pill(), recall(places(), placeKey(w.key), w.key.providerId), taken);
    auto shown = std::make_unique<Shown>();
    shown->key = w.key;
    shown->window = std::make_unique<platform::ProviderWindow>();
    if (!shown->window->create(g_hooks.instance, g_hooks.compositor, *g_hooks.device, frame, tokens().windowMinSize, w.spec.keepOnTop,
                               w.spec.resizable, w.spec.maximizable, callbacksFor(w.key))) {
        g_hooks.registry->windowClosed(w.key); // logged; closed for its provider, not tried again and again
        return;
    }
    Shown& s = *shown;
    s.raised = w.raised;
    g_shown[w.key] = std::move(shown); // before anything that sends messages finds it
    s.window->setGlass(g_glass);
    // Drawn in the next queue turn (decision 012), so the click that opened it
    // is answered at once: building every row's text first froze it. Shown
    // once that's committed: shown first, it flashed what was behind it.
    WindowKey key = w.key;
    bool onTop = w.spec.keepOnTop;
    auto draw = [key, onTop] {
        guarded("drawing a new window", [&] {
            Shown* shown = find(key);
            if (!shown) return;
            shown->window->drawThenShow([shown] { relayout(*shown); }, [onTop] {
                if (onTop) platform::raiseIsland(); // the island stays over its windows
            });
        });
    };
    auto queue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    if (!queue || !queue.TryEnqueue(winrt::Windows::System::DispatcherQueuePriority::Low, draw)) draw();
    platform::logLine("window opened: %s", placeKey(w.key).c_str());
}

void applyWindows()
{
    g_syncQueued = false;
    if (!g_hooks.registry) return;
    const Windows& windows = g_hooks.registry->windows();
    std::vector<WindowKey> gone;
    for (const auto& [key, s] : g_shown)
        if (!findWindow(windows, key)) gone.push_back(key);
    for (const WindowKey& key : gone) {
        rememberPlace(*g_shown[key]);
        g_shown.erase(key); // its window is destroyed with it
        platform::logLine("window closed: %s", placeKey(key).c_str());
    }
    std::vector<OpenWindow> open = windows.open; // a copy: opening one may change the registry's
    for (const OpenWindow& w : open) {
        Shown* s = find(w.key);
        if (!s) {
            openWindowFor(w);
            continue;
        }
        relayout(*s);
        showHover(*s);
        if (w.raised != s->raised) {
            s->raised = w.raised;
            s->window->raise();
            if (w.spec.keepOnTop) platform::raiseIsland();
        }
    }
}

} // namespace

void setWindowHooks(WindowHooks hooks) { g_hooks = std::move(hooks); }

void syncWindows()
{
    if (g_syncQueued) return;
    g_syncQueued = true;
    auto queue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    auto apply = [] { guarded("updating windows", applyWindows); };
    if (!queue || !queue.TryEnqueue(apply)) apply(); // no queue (shutting down): now
}

void refreshWindows()
{
    bool coloursOnly = g_drawnFor && onlyColorsDiffer(*g_drawnFor, tokens());
    g_drawnFor = tokens();
    for (auto& [key, s] : g_shown) {
        s->window->refresh(coloursOnly);
        relayout(*s);
    }
}

void setWindowsGlass(bool on)
{
    if (on == g_glass) return;
    g_glass = on;
    for (auto& [key, s] : g_shown) s->window->setGlass(on);
}

void closeAllWindows()
{
    for (auto& [key, s] : g_shown) rememberPlace(*s);
    g_shown.clear();
}

} // namespace cup::app
