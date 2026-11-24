#include "app/keyboard_input.h"

#include "app/file_picker.h"

#include "app/menu_input.h"

#include "platform/foreground.h"
#include "providers/search/search_provider.h"

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

// Enter or Space: a click on what has focus.
void activate()
{
    std::optional<Focusable> focused;
    if (!g_focus.empty()) focused = findFocusable(scene(), g_focus);
    if (!focused) return;
    if (focused->id.rfind("tab:", 0) == 0) {
        g_hooks.clickTab(focused->activityId);
    } else if (auto hit = hitTest(scene(), focused->point)) {
        g_hooks.clickElement(*hit);
    }
}

// Ctrl+Tab and Ctrl+Shift+Tab (decision 033): the next or previous page
// of the row's list, as a click on its icon.
void switchTab(bool back)
{
    auto to = back ? previousTab(scene()) : nextTab(scene());
    if (to) g_hooks.clickTab(*to);
}

// A system-wide shortcut was pressed (decision 032). The island comes to
// the front with it, so the arrows work at once and a click elsewhere closes
// it like any other time: over Start or search too, which close (decision 089).
void run(Command command)
{
    switch (command) {
    case Command::OpenIsland: // the default page's card (decision 170); pressed again while open, it closes, like Esc
        if (isExpanded(state())) {
            dispatch(EscapePressed{}, "shortcut");
            return;
        }
        dispatch(IslandOpened{}, "shortcut");
        if (isExpanded(state())) platform::bringToFront(g_hooks.window);
        return;
    case Command::OpenActivity: // the pill's activity's card; again on it, closes (decision 050)
        dispatch(ActivityOpened{}, "shortcut");
        if (!state().expandedId.empty()) platform::bringToFront(g_hooks.window);
        return;
    case Command::OpenSearch: { // search's card, typing in its field at once; again, it closes (decision 079)
        if (filePickerShortcut()) return; // with a file dialog in front: the card under it, as its pill opens it (decision 181)
        const std::string id = SearchProvider::kPanelId;
        if (state().expandedId == id) {
            dispatch(CardCloseRequested{ id }, "shortcut");
            return;
        }
        dispatch(CardOpened{ id }, "shortcut");
        if (state().expandedId != id) return;
        platform::bringToFront(g_hooks.window); // focused: the card's field has the keys already (Content::fieldFirst)
        dispatch(AllSelected{}, "shortcut"); // open already: its words selected, typing replaces them
        return;
    }
    }
}

} // namespace

void setKeyboardHooks(KeyboardHooks hooks) { g_hooks = std::move(hooks); }

std::optional<Focusable> keyFocus()
{
    // Focus is on an open card: once it closes, the next one starts afresh,
    // and the pill's own buttons in Compact never light up from an old focus.
    if (mode(state()) != Mode::Expanded) g_focus.clear();
    if (g_focus.empty()) return std::nullopt;
    if (auto focused = findFocusable(scene(), g_focus)) return focused;
    if (state().draft) return std::nullopt; // typing: the field has the keys
    // What had focus is gone: a card or page opened, a tab switched, Esc went
    // back. The keys carry on there, at the first item.
    std::optional<Focusable> next = firstFocus(scene());
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
bool recordKey(WPARAM key, bool inSettings)
{
    if (!g_capturing) return false;
    if (!g_hooks.settings) {
        g_capturing = false;
        return false;
    }
    // Pressed in Settings' window: the new keys (decision 104). Anywhere else, or Esc: it stops waiting.
    if (!inSettings || key == VK_ESCAPE) g_hooks.settings->keysPressed(std::nullopt);
    else if (auto chord = platform::chordPressed(key)) g_hooks.settings->keysPressed(chord);
    return true;
}

bool navigationKey(WPARAM key)
{
    bool shift = GetKeyState(VK_SHIFT) < 0, ctrl = GetKeyState(VK_CONTROL) < 0;
    bool expanded = mode(state()) == Mode::Expanded; // focus only moves on an open card
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
    case VK_RIGHT:
        to = moveFocus(scene(), g_focus, Arrow::Right);
        if (!to && expanded && openMenuByKeys()) return true; // nothing further right on a row with a menu: its menu (decision 086)
        break;
    case VK_TAB: to = shift ? previousFocus(scene(), g_focus) : nextFocus(scene(), g_focus); break;
    case VK_HOME: to = firstFocus(scene()); break;
    case VK_END: to = lastFocus(scene()); break;
    default: return false;
    }
    if (expanded) land(to);
    return true;
}

bool fieldCardKey(WPARAM key)
{
    if (key != VK_UP && key != VK_DOWN) return false;
    Arrow arrow = key == VK_UP ? Arrow::Up : Arrow::Down;
    // The last rows out of view: scrolled in before Down stops (decision 033).
    if (!g_focus.empty()) {
        if (auto scroll = scrollInstead(scene(), g_focus, arrow)) {
            dispatch(*scroll, "keys: scroll the list");
            dispatch(PointerMoved{}, "keys");
            return true;
        }
    }
    // Typing, the first row is lit already (defaultRow): Down goes past it, and
    // Up onto it is typing again, so each press visibly moves (decision 079).
    std::optional<Focusable> lit = defaultRow(scene());
    std::optional<Focusable> to = moveAmongRows(scene(), g_focus.empty() && lit ? lit->id : g_focus, arrow);
    if (to && lit && to->id == lit->id) to.reset();
    if (to) land(to);
    else { // up past the first row: back to typing, the first row lit
        g_focus.clear();
        dispatch(PointerMoved{}, "keys");
    }
    return true;
}

bool activateFocused()
{
    if (g_focus.empty()) return false;
    auto focused = findFocusable(scene(), g_focus);
    if (!focused || fieldStop(scene(), *focused)) return false;
    activate();
    return true;
}

void dropKeyFocus() { g_focus.clear(); }

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
