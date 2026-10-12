#include "core/shortcuts.h"

namespace cup {

const std::vector<Shortcut>& shortcuts()
{
    static const std::vector<Shortcut> all{
        { "Ctrl+X", "Open Home", Chord{ .key = 'X', .ctrl = true }, Command::OpenHome },
        { "Arrows", "Move between items" },
        { "Enter or Space", "Select" },
        { "Esc", "Close" },
    };
    return all;
}

std::optional<Command> commandFor(const Chord& chord)
{
    for (const Shortcut& s : shortcuts())
        if (s.chord == chord) return s.command;
    return std::nullopt;
}

} // namespace cup
