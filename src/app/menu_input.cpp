#include "app/menu_input.h"

#include "app/keyboard_input.h"
#include "core/menu.h"
#include "core/text_edit.h"

#include <optional>
#include <utility>

namespace cup::app {
namespace {

MenuHooks g_hooks;
std::optional<std::size_t> g_pressed; // the item a press on the menu went down on
bool g_pressTaken = false;            // a press the menu took: its release is the menu's too
bool g_swallow = false;

const IslandState& state() { return g_hooks.state(); }
const Scene& scene() { return g_hooks.scene(); }
void dispatch(const Event& event, const char* reason) { g_hooks.dispatch(event, reason); }
const Menu* open() { return state().card.menu ? &*state().card.menu : nullptr; }

// Closed first, then its action: whatever the provider does (a new query, closing the card) starts from no menu.
void choose(std::size_t index)
{
    const Menu* m = open();
    if (!m || index >= m->items.size() || !m->items[index].enabled) return;
    Action action{ m->providerId, m->activityId, m->items[index].actionId, 0 };
    dispatch(MenuClosed{}, "menu item");
    g_hooks.act(action);
}

// The row the keys are on (decision 086): the one highlighted, else on a card
// that is its field, the one Enter opens (decision 079). "" for none.
std::string litRow()
{
    auto focused = keyFocus();
    if (!focused && escapeCloses(state())) focused = defaultRow(scene());
    if (!focused || focused->id.rfind("el:", 0) != 0) return {};
    return rowKeyOf(focused->id.substr(3));
}

void light(std::optional<std::size_t> item, const char* reason)
{
    if (item) dispatch(MenuLit{ item }, reason);
}

} // namespace

void setMenuHooks(MenuHooks hooks) { g_hooks = std::move(hooks); }

void menuRightClick(Vec2 point)
{
    std::string row = rowAt(scene(), point);
    if (open()) dispatch(MenuClosed{}, "right-click elsewhere");
    if (!row.empty()) dispatch(MenuOpened{ scene().activityId, row, point }, "right-click"); // core opens it only where there's a menu
}

bool openMenuByKeys()
{
    if (!state().focusedFloatId.empty()) return false; // a floating card's menu isn't drawn yet
    std::string row = litRow();
    if (row.empty()) return false;
    dispatch(MenuOpened{ scene().activityId, row, std::nullopt }, "menu key");
    return open() != nullptr;
}

bool menuPress(Vec2 point)
{
    g_pressTaken = false;
    if (!open() || !scene().menu) return false;
    g_pressTaken = true;
    if (scene().menu->frame.contains(point)) {
        g_pressed = menuItemAt(*scene().menu, point);
        return true;
    }
    g_pressed.reset();
    dispatch(MenuClosed{}, "press outside the menu"); // like Windows' menus: the press only closes it
    return true;
}

bool menuRelease(Vec2 point)
{
    if (!std::exchange(g_pressTaken, false)) return false;
    auto pressed = std::exchange(g_pressed, std::nullopt);
    if (pressed && scene().menu && menuItemAt(*scene().menu, point) == pressed) choose(*pressed);
    return true;
}

void menuPointerMoved(Vec2 point)
{
    const Menu* m = open();
    if (!m || !scene().menu || !scene().menu->frame.contains(point)) return; // off it: what was lit stays, as in Windows
    auto under = menuItemAt(*scene().menu, point);
    if (under != m->lit) dispatch(MenuLit{ under }, "pointer on menu");
}

bool menuKey(WPARAM vk)
{
    if (!state().focusedFloatId.empty()) return false;
    bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0, alt = GetKeyState(VK_MENU) < 0;
    // An item's shortcut, on the lit row (the menu's own row while it's open).
    const Menu* m = open();
    std::string row = m ? m->row : litRow();
    const Content* content = row.empty() ? nullptr : cardContent(state(), scene().activityId);
    Chord chord{ .key = static_cast<int>(vk), .ctrl = ctrl, .alt = alt, .shift = shift };
    bool plainEnter = vk == VK_RETURN && !ctrl && !alt && !shift; // Open's: Enter already does it
    // Ctrl+C copies the file only when no text is selected (decision 086): selected, it's the text's.
    bool textCopy = vk == 'C' && ctrl && !shift && !alt && state().draft && !selectedText(*state().draft).empty();
    if (content && !plainEnter && !textCopy)
        if (const MenuItem* item = itemForChord(*content, row, chord)) {
            Action action{ m ? m->providerId : std::string(), scene().activityId, item->actionId, 0 };
            if (!m) // closed: the row's provider, from the card that's shown
                for (const Placed& p : scene().elements)
                    if (rowKeyOf(p.key) == row) action.providerId = p.providerId;
            g_swallow = true; // first: the action may pump messages (ShellExecute), and the key's WM_CHAR with them
            if (m) dispatch(MenuClosed{}, "menu shortcut");
            g_hooks.act(action);
            return true;
        }
    if (!m) return false;
    switch (vk) {
    case VK_UP: light(menuStep(*m, -1), "menu keys"); return true;
    case VK_DOWN: light(menuStep(*m, 1), "menu keys"); return true;
    case VK_HOME: light(menuEnd(*m, false), "menu keys"); return true;
    case VK_END: light(menuEnd(*m, true), "menu keys"); return true;
    case VK_RETURN:
    case VK_SPACE:
        // Set before choosing: starting what was picked can pump messages (ShellExecute,
        // a COM call), and the Enter's WM_CHAR would then reach the field and open the row too.
        g_swallow = true;
        if (m->lit) choose(*m->lit);
        return true;
    case VK_ESCAPE:
    case VK_LEFT: dispatch(MenuClosed{}, "menu keys"); return true; // back to its row, which keeps the highlight
    case VK_RIGHT:
    case VK_TAB:
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU: return true; // no submenus; a modifier may start a shortcut
    default: break;
    }
    if (vk >= 'A' && vk <= 'Z' && !ctrl && !alt)
        if (auto at = menuLetter(*m, static_cast<char>(vk))) {
            light(at, "menu letter");
            g_swallow = true;
            return true;
        }
    dispatch(MenuClosed{}, "typing"); // anything else: the menu goes and the key does what it does (types)
    return false;
}

bool swallowChar() { return std::exchange(g_swallow, false); }
void clearCharSwallow() { g_swallow = false; }

} // namespace cup::app
