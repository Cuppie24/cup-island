#include "app/keyboard_input.h"

#include "core/keyboard.h"
#include "core/shortcuts.h"
#include "platform/hotkeys.h"
#include "providers/system/settings_provider.h"

#include <utility>

namespace cup::app {
namespace {

KeyboardHooks g_hooks;
std::string g_focus;               // a Focusable's id; "" = none, the pointer leads
std::optional<Vec2> g_lastPointer; // the last move's point, to tell a real move from a repeat
bool g_capturing = false;

const IslandState& state() { return g_hooks.state(); }
const Scene& scene() { return g_hooks.scene(); }
void dispatch(const Event& event, const char* reason) { g_hooks.dispatch(event, reason); }

// The arrows (decision 031): move focus on the card or Home that's open. A
// row out of its list's window is scrolled in first. Pressing keys counts as
// using the island, like moving the pointer: Expanded doesn't time out.
void move(Arrow arrow)
{
    if (mode(state()) != Mode::Expanded) return;
    if (auto next = moveFocus(scene(), g_focus, arrow)) {
        g_focus = next->id;
        if (next->list) dispatch(ListRevealed{ next->activityId, next->list->id, next->list->row }, "keys: scroll to focus");
    }
    dispatch(PointerMoved{}, "keys");
}

// Enter or Space: a click on what has focus. If that opened something else
// (a card, a page), focus goes to its first item, so the keys carry on there.
void activate()
{
    std::optional<Focusable> focused;
    if (!g_focus.empty()) focused = findFocusable(scene(), g_focus);
    if (!focused) return;
    if (focused->id.rfind("tab:", 0) == 0) {
        g_hooks.clickTab(focused->activityId);
    } else if (focused->id.rfind("tile:", 0) == 0) {
        if (const Tile* tile = tileAt(scene(), focused->point)) g_hooks.clickTile(*tile);
    } else if (auto hit = hitTest(scene(), focused->point)) {
        g_hooks.clickElement(*hit);
    }
    if (!state().draft && !findFocusable(scene(), g_focus)) {
        auto first = moveFocus(scene(), "", Arrow::Down);
        g_focus = first ? first->id : std::string();
        g_hooks.updateHover();
    }
}

// A system-wide shortcut was pressed (decision 032). The island comes to
// the front with it (Windows allows that to whoever gets a hotkey), so the
// arrows work at once and a click elsewhere closes it like any other time.
void run(Command command)
{
    switch (command) {
    case Command::OpenHome:
        if (state().homeOpen) { // pressed again: it closes, like Esc
            dispatch(EscapePressed{}, "shortcut");
            return;
        }
        dispatch(HomeOpened{}, "shortcut");
        if (state().homeOpen) SetForegroundWindow(g_hooks.window);
        return;
    }
}

} // namespace

void setKeyboardHooks(KeyboardHooks hooks) { g_hooks = std::move(hooks); }

std::optional<Vec2> keyFocusPoint()
{
    if (g_focus.empty()) return std::nullopt;
    auto focused = findFocusable(scene(), g_focus);
    return focused ? std::optional<Vec2>(focused->point) : std::nullopt;
}

void pointerMoved(Vec2 canvasPoint)
{
    if (g_lastPointer != canvasPoint) g_focus.clear();
    g_lastPointer = canvasPoint;
}

void captureKeys(bool on) { g_capturing = on; }

// Settings is recording a shortcut: the next key is its new keys, Esc leaves
// it. A modifier on its own is part of what's being pressed: wait for more.
bool recordKey(WPARAM key)
{
    if (!g_capturing) return false;
    if (!g_hooks.settings) {
        g_capturing = false;
        return false;
    }
    bool settingsShown = !state().homeOpen && state().expandedId == SettingsProvider::kPanelId;
    if (!settingsShown || key == VK_ESCAPE) g_hooks.settings->keysPressed(std::nullopt);
    else if (auto chord = platform::chordPressed(key)) g_hooks.settings->keysPressed(chord);
    else return true;
    dispatch(PointerMoved{}, "shortcut keys"); // busy here: keep the card open
    return true;
}

bool navigationKey(WPARAM key)
{
    switch (key) {
    case VK_UP: move(Arrow::Up); return true;
    case VK_DOWN: move(Arrow::Down); return true;
    case VK_LEFT: move(Arrow::Left); return true;
    case VK_RIGHT: move(Arrow::Right); return true;
    case VK_RETURN:
    case VK_SPACE: activate(); return true;
    default: return false;
    }
}

void hotkeyPressed(WPARAM id)
{
    auto command = platform::hotkeyCommand(id);
    if (!command) return;
    // Recording a shortcut, a registered one's keys arrive here instead of as
    // a key press: they're what was pressed (its own: no change; another's: a conflict).
    if (g_capturing && g_hooks.settings) {
        g_hooks.settings->shortcutPressed(*command);
        dispatch(PointerMoved{}, "shortcut keys");
        return;
    }
    run(*command);
}

} // namespace cup::app
