#pragma once
// Windows' own theme (decision 150): Settings and providers' other windows
// drawn like the island, in Windows' light or dark mode, or in a theme picked
// for them. Which theme that comes to is decided here, purely: the app reads
// Windows' mode and draws the windows in what this returns.

#include "core/tokens.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

// Where windows take their colours: the island's (its theme and music's
// colours, as before), Windows' app mode, or a theme of their own.
enum class WindowThemeMode { Island, System, Manual };

// As the store keeps it: "island", "system", "manual"; anything else is Island.
const char* windowThemeText(WindowThemeMode mode);
WindowThemeMode parseWindowThemeMode(const std::string& text);

// The theme windows are drawn in, by name; nothing: the island's tokens as
// they are. System: the first light theme in tokens.json's order when Windows'
// apps are light, else the first dark one (none of that kind: the island's).
// Manual: `manual` if tokens.json has it, else the first theme.
std::optional<std::string> windowThemeFor(WindowThemeMode mode, const std::string& manual, bool systemLight, const std::vector<Theme>& themes);

} // namespace cup
