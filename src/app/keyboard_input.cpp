#include "app/keyboard_input.h"

#include "core/keyboard.h"
#include "core/shortcuts.h"
#include "core/tokens.h"
#include "platform/hotkeys.h"
#include "platform/interaction_layer.h"
#include "providers/system/settings_provider.h"

#include <utility>

namespace cup::app {
namespace {

KeyboardHooks g_hooks;
std::string g_focus;               // a Focusable's id; "" = none, the pointer leads
std::string g_homeTile;            // the Home widget the keys last clicked: focus goes back to it on Home (decision 033)
std::optional<Vec2> g_lastPointer; // the last move's point, to tell a real move from a repeat
bool g_capturing = false;

const IslandState& state() { return g_hooks.state(); }
const Scene& scene() { return g_hooks.scene(); }
void dispatch(const Event& event, const char* reason) { g_hooks.dispatch(event, reason); }

// Focus lands on `f`: a row out of its list's window is scrolled in first.
// Pressing keys counts as using the island, like moving the pointer:
// Expanded doesn't time out.
void land(const std::optional<Focusable>& f)
{
    if (f) {
        g_focus = f->id;
        if (f->list) dispatch(ListRevealed{ f->activityId, f->list->id, f->list->row }, "keys: scroll to focus");
    }
    dispatch(PointerMoved{}, "keys");
}

// Enter or Space: a click on what has focus. A static widget takes it and
// nothing happens, as with a click.
void activate()
{
    std::optional<Focusable> focused;
    if (!g_focus.empty()) focused = findFocusable(scene(), g_focus);
    if (!focused) return;
    if (focused->id.rfind("tab:", 0) == 0) {
        g_hooks.clickTab(focused->activityId);
    } else if (focused->id.rfind("tile:", 0) == 0) {
        g_homeTile = focused->id;
        if (const Tile* tile = tileAt(scene(), focused->point)) g_hooks.clickTile(*tile);
    } else if (auto hit = hitTest(scene(), focused->point)) {
        g_hooks.clickElement(*hit);
    }
}

// Ctrl+Tab and Ctrl+Shift+Tab (decision 033): the next or previous tab,
// Home and the activities in turn, as a click on it.
void switchTab(bool back)
{
    auto to = back ? previousTab(scene()) : nextTab(scene());
    if (to) g_hooks.clickTab(*to);
}

// 1 to 9 on Home (decision 033): its nth widget gets focus and is clicked,
// as if arrowed to and Enter pressed. A static one just shows focus.
void openWidget(std::size_t n)
{
    auto widget = homeWidget(scene(), n);
    if (!widget) return;
    land(widget);
    activate();
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

std::optional<Focusable> keyFocus()
{
    // Focus is on an open card or Home: once it closes, the next one starts afresh,
    // and the pill's own buttons in Compact never light up from an old focus.
    if (mode(state()) != Mode::Expanded) {
        g_focus.clear();
        g_homeTile.clear();
    }
    if (g_focus.empty()) return std::nullopt;
    if (auto focused = findFocusable(scene(), g_focus)) return focused;
    if (state().draft) return std::nullopt; // typing: the field has the keys
    // What had focus is gone: a card or page opened, a tab switched, Esc went
    // back. The keys carry on there: back on Home, at the widget they last
    // clicked (decision 033); anywhere else, at the first item.
    std::optional<Focusable> next;
    if (state().homeOpen && !g_homeTile.empty()) next = findFocusable(scene(), g_homeTile);
    if (!next) next = firstFocus(scene());
    g_focus = next ? next->id : std::string();
    return next;
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
    bool shift = GetKeyState(VK_SHIFT) < 0, ctrl = GetKeyState(VK_CONTROL) < 0;
    bool expanded = mode(state()) == Mode::Expanded; // focus only moves on an open card or Home
    if (key == VK_RETURN || key == VK_SPACE) {
        activate();
        return true;
    }
    if (key == VK_TAB && ctrl) {
        if (expanded) switchTab(shift);
        return true;
    }
    // The rest move focus. The first press of any shows where it is: each
    // starts at the first item when nothing has focus.
    std::optional<Focusable> to;
    switch (key) {
    case VK_UP:
    case VK_DOWN: {
        Arrow arrow = key == VK_UP ? Arrow::Up : Arrow::Down;
        // At a list's last stop, rows with nothing to click may still be out
        // of view: scroll them in before leaving (decision 033).
        if (auto scroll = expanded ? scrollInstead(scene(), g_focus, arrow) : std::nullopt) {
            dispatch(*scroll, "keys: scroll the list");
            dispatch(PointerMoved{}, "keys");
            return true;
        }
        to = moveFocus(scene(), g_focus, arrow);
        break;
    }
    case VK_LEFT: to = moveFocus(scene(), g_focus, Arrow::Left); break;
    case VK_RIGHT: to = moveFocus(scene(), g_focus, Arrow::Right); break;
    case VK_TAB: to = shift ? previousFocus(scene(), g_focus) : nextFocus(scene(), g_focus); break;
    case VK_HOME: to = firstFocus(scene()); break;
    case VK_END: to = lastFocus(scene()); break;
    default: {
        // 1 to 9, on the top row or the number pad: Home's widgets, bare keys only.
        bool digit = (key >= '1' && key <= '9') || (key >= VK_NUMPAD1 && key <= VK_NUMPAD9);
        if (!digit || ctrl || !expanded || !state().homeOpen) return false;
        openWidget(key >= VK_NUMPAD1 ? key - VK_NUMPAD1 + 1 : key - '0');
        return true;
    }
    }
    if (expanded) land(to);
    return true;
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
    // Held, it daydreams (decision 050). Windows sends no key-up for a
    // hotkey, so the key is checked until it's let go: only while held.
    if (*command != Command::OpenHome) return;
    dispatch(HomeShortcutHeld{}, "shortcut held");
    platform::setTimer(platform::TimerId::ShortcutPoll, tokens().shortcutPoll);
}

void pollHeldShortcut()
{
    if (platform::hotkeyHeld(Command::OpenHome)) {
        platform::setTimer(platform::TimerId::ShortcutPoll, tokens().shortcutPoll);
        return;
    }
    dispatch(HomeShortcutReleased{}, "shortcut let go");
}

} // namespace cup::app
