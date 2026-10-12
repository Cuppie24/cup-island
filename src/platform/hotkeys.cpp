#include "platform/hotkeys.h"

namespace cup::platform {
namespace {

// Ids 1.. per command, in commands()' order (0 is reserved by Windows' convention).
int idOf(Command command)
{
    const auto& all = commands();
    for (std::size_t i = 0; i < all.size(); ++i)
        if (all[i].command == command) return static_cast<int>(i) + 1;
    return 0;
}

bool held(int key) { return GetKeyState(key) < 0; }

} // namespace

bool registerHotkey(HWND hwnd, Command command, const Chord& chord)
{
    int id = idOf(command);
    UnregisterHotKey(hwnd, id); // fails harmlessly when it had none
    UINT modifiers = MOD_NOREPEAT; // held down, it fires once (Windows 7+)
    if (chord.ctrl) modifiers |= MOD_CONTROL;
    if (chord.alt) modifiers |= MOD_ALT;
    if (chord.shift) modifiers |= MOD_SHIFT;
    if (chord.win) modifiers |= MOD_WIN;
    return RegisterHotKey(hwnd, id, modifiers, static_cast<UINT>(chord.key)) != 0;
}

void unregisterHotkeys(HWND hwnd)
{
    for (std::size_t i = 0; i < commands().size(); ++i) UnregisterHotKey(hwnd, static_cast<int>(i) + 1);
}

std::optional<Command> hotkeyCommand(WPARAM id)
{
    const auto& all = commands();
    if (id < 1 || id > all.size()) return std::nullopt;
    return all[id - 1].command;
}

std::optional<Chord> chordPressed(WPARAM key)
{
    int k = static_cast<int>(key);
    if (!shortcutKey(k)) return std::nullopt;
    return Chord{ .key = k, .ctrl = held(VK_CONTROL), .alt = held(VK_MENU), .shift = held(VK_SHIFT),
                  .win = held(VK_LWIN) || held(VK_RWIN) };
}

} // namespace cup::platform
