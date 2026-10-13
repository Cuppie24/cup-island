#include "core/shortcuts.h"

#include <algorithm>

namespace cup {
namespace {

std::string trimmed(std::string_view text)
{
    std::size_t a = text.find_first_not_of(" \t\r");
    if (a == std::string_view::npos) return {};
    std::size_t b = text.find_last_not_of(" \t\r");
    return std::string(text.substr(a, b - a + 1));
}

bool same(std::string_view a, std::string_view b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return (x >= 'a' && x <= 'z' ? x - 32 : x) == (y >= 'a' && y <= 'z' ? y - 32 : y);
           });
}

std::string keyName(int key)
{
    if (key == kSpace) return "Space";
    if (key >= kF1 && key < kF1 + 12) return "F" + std::to_string(key - kF1 + 1);
    return std::string(1, static_cast<char>(key));
}

std::optional<int> keyNamed(std::string_view name)
{
    if (same(name, "Space")) return kSpace;
    if (name.size() == 1) {
        char c = name[0] >= 'a' && name[0] <= 'z' ? static_cast<char>(name[0] - 32) : name[0];
        if (shortcutKey(c)) return c;
    }
    if (name.size() >= 2 && (name[0] == 'F' || name[0] == 'f')) {
        std::string digits(name.substr(1));
        if (digits.find_first_not_of("0123456789") == std::string::npos && digits.size() <= 2) {
            int n = std::stoi(digits);
            if (n >= 1 && n <= 12) return kF1 + n - 1;
        }
    }
    return std::nullopt;
}

} // namespace

bool shortcutKey(int key)
{
    return (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == kSpace || (key >= kF1 && key < kF1 + 12);
}

std::string chordText(const Chord& c)
{
    std::string text;
    if (c.ctrl) text += "Ctrl+";
    if (c.alt) text += "Alt+";
    if (c.shift) text += "Shift+";
    if (c.win) text += "Win+";
    return text + keyName(c.key);
}

std::optional<Chord> parseChord(std::string_view text)
{
    Chord c;
    std::string_view rest = text;
    for (;;) {
        std::size_t plus = rest.find('+');
        std::string part = trimmed(rest.substr(0, plus));
        if (plus == std::string_view::npos) {
            auto key = keyNamed(part);
            if (!key) return std::nullopt;
            c.key = *key;
            return c;
        }
        if (same(part, "Ctrl")) c.ctrl = true;
        else if (same(part, "Alt")) c.alt = true;
        else if (same(part, "Shift")) c.shift = true;
        else if (same(part, "Win")) c.win = true;
        else return std::nullopt;
        rest = rest.substr(plus + 1);
    }
}

const std::vector<CommandInfo>& commands()
{
    static const std::vector<CommandInfo> all{
        { Command::OpenHome, "home", "Open Home", Chord{ .key = 'Q', .ctrl = true } }, // takes Ctrl+Q (Quit in some apps) from other apps: the user's pick (decision 032)
    };
    return all;
}

const std::vector<FixedKey>& fixedKeys()
{
    static const std::vector<FixedKey> all{
        { "Arrows", "Move between items" },
        { "Tab or Shift+Tab", "Next or previous item" },
        { "Home or End", "First or last item" },
        { "Ctrl+Tab", "Next tab" },
        { "Ctrl+Shift+Tab", "Previous tab" },
        { "1 to 9", "Open a widget on Home" },
        { "Enter or Space", "Select" },
        { "Esc", "Back or close" },
        { "Ctrl+W", "Close the activity" },
    };
    return all;
}

const CommandInfo& infoOf(Command command)
{
    for (const CommandInfo& c : commands())
        if (c.command == command) return c;
    return commands().front();
}

Bindings defaultBindings()
{
    Bindings b;
    for (const CommandInfo& c : commands()) b.push_back({ c.command, c.standard });
    return b;
}

Chord chordOf(const Bindings& bindings, Command command)
{
    for (const Binding& b : bindings)
        if (b.command == command) return b.chord;
    return infoOf(command).standard;
}

std::string saveBindings(const Bindings& bindings)
{
    std::string text;
    for (const Binding& b : bindings) text += infoOf(b.command).id + "=" + chordText(b.chord) + "\n";
    return text;
}

Bindings loadBindings(std::string_view text)
{
    Bindings bindings = defaultBindings();
    while (!text.empty()) {
        std::size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
        std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        std::string id = trimmed(line.substr(0, eq));
        auto chord = parseChord(line.substr(eq + 1));
        if (!chord) continue; // unreadable: the default stays
        for (Binding& b : bindings)
            if (infoOf(b.command).id == id && bindingProblem(bindings, b.command, *chord).empty()) b.chord = *chord;
    }
    return bindings;
}

std::string bindingProblem(const Bindings& bindings, Command command, const Chord& chord)
{
    if (!shortcutKey(chord.key)) return chordText(chord) + " can't be a shortcut";
    if (!chord.ctrl && !chord.alt && !chord.win) return "A shortcut needs Ctrl, Alt or Win";
    for (const Binding& b : bindings)
        if (b.command != command && b.chord == chord) return chordText(chord) + " is already " + infoOf(b.command).does;
    return {};
}

} // namespace cup
