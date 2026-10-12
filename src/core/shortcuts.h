#pragma once
// Keyboard shortcuts (decision 031): one table, so what a key does and what
// Settings lists can't drift apart. Pure: the app turns a key press into a
// Chord and runs the Command; the island needs keyboard focus (it has it
// once clicked), they're not system-wide hotkeys.

#include <optional>
#include <string>
#include <vector>

namespace cup {

enum class Command { OpenHome };

// A key with its modifiers. `key` is an upper-case letter or digit ('X').
struct Chord {
    char key = 0;
    bool ctrl = false, shift = false, alt = false;
    bool operator==(const Chord&) const = default;
};

// One line of Settings' list: the keys as written, and what they do. A
// shortcut has a chord and a command; the rest describe keys handled where
// they apply (the arrows, Enter, Esc).
struct Shortcut {
    std::string keys; // "Ctrl+X"
    std::string does; // "Open Home"
    std::optional<Chord> chord;
    std::optional<Command> command;
};

const std::vector<Shortcut>& shortcuts(); // in the order Settings lists them

std::optional<Command> commandFor(const Chord& chord);

} // namespace cup
