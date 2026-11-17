#include "providers/system/window_theme.h"

#include <algorithm>

namespace cup {

const char* windowThemeText(WindowThemeMode mode)
{
    return mode == WindowThemeMode::System ? "system" : mode == WindowThemeMode::Manual ? "manual" : "island";
}

WindowThemeMode parseWindowThemeMode(const std::string& text)
{
    if (text.rfind("system", 0) == 0) return WindowThemeMode::System; // a hand-edited file's newline after it
    if (text.rfind("manual", 0) == 0) return WindowThemeMode::Manual;
    return WindowThemeMode::Island;
}

std::optional<std::string> windowThemeFor(WindowThemeMode mode, const std::string& manual, bool systemLight, const std::vector<Theme>& themes)
{
    if (themes.empty() || mode == WindowThemeMode::Island) return std::nullopt;
    if (mode == WindowThemeMode::System) {
        auto it = std::find_if(themes.begin(), themes.end(), [&](const Theme& t) { return t.light == systemLight; });
        if (it == themes.end()) return std::nullopt;
        return it->name;
    }
    auto it = std::find_if(themes.begin(), themes.end(), [&](const Theme& t) { return t.name == manual; });
    return it == themes.end() ? themes.front().name : it->name;
}

} // namespace cup
