#pragma once
// Keyboard shortcuts (decisions 031, 032). Commands are system-wide hotkeys,
// each rebindable in Settings; the keys that move around the island (the
// arrows, Enter, Esc) are fixed and only listed. Pure: names, the text form
// of a chord, the saved format, and whether a new chord may be bound. The
// app registers the chords with Windows and runs the commands.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cup {

enum class Command { OpenHome, OpenActivity };

// A key with its modifiers. `key` uses Windows' virtual-key numbers for the
// keys a shortcut may use: 'A'..'Z', '0'..'9', kSpace, kF1..kF1 + 11. Core
// only compares and names them; the app passes them straight through.
struct Chord {
    int key = 0;
    bool ctrl = false, alt = false, shift = false, win = false;
    bool operator==(const Chord&) const = default;
};
constexpr int kSpace = 0x20;
constexpr int kF1 = 0x70;

bool shortcutKey(int key);                         // a key a shortcut may end in (not a modifier, not Enter or the arrows)
std::string chordText(const Chord& chord);         // "Ctrl+Alt+Space", "Ctrl+Q"
std::optional<Chord> parseChord(std::string_view text); // chordText's inverse; nothing if it isn't one

// A command: its name in the saved file, what Settings calls it, and its default keys.
struct CommandInfo {
    Command command;
    std::string id;   // "home"
    std::string does; // "Open Home"
    Chord standard;
};
const std::vector<CommandInfo>& commands(); // in the order Settings lists them

// Keys that work on the island and can't be changed, for Settings' list.
struct FixedKey {
    std::string keys, does;
};
const std::vector<FixedKey>& fixedKeys();

// The keys each command is bound to, in commands()' order.
struct Binding {
    Command command;
    Chord chord;
    bool operator==(const Binding&) const = default;
};
using Bindings = std::vector<Binding>;

Bindings defaultBindings();
std::string saveBindings(const Bindings& bindings);    // "home=Ctrl+Q", a line each
Bindings loadBindings(std::string_view text);          // defaults for whatever is missing or unreadable

// Why `chord` can't be bound to `command`, for Settings to say; empty: it can.
// System-wide, a chord needs Ctrl, Alt or Win (a bare key would stop that
// key typing anywhere), and two commands can't share one.
std::string bindingProblem(const Bindings& bindings, Command command, const Chord& chord);

const CommandInfo& infoOf(Command command);
Chord chordOf(const Bindings& bindings, Command command);

} // namespace cup
