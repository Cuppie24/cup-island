#pragma once
// The Settings card (decisions 030, 032): built from a SettingsView by one
// pure function. A list of settings, or a page of one setting's choices
// (themes, displays) or the keyboard shortcuts.

#include "core/island.h"
#include "core/shortcuts.h"
#include "providers/update/release.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

// What the card's buttons send. A choice's action is its prefix and its index in the page's list.
namespace settings_action {
inline constexpr const char* kOpenThemes = "theme.open";
inline constexpr const char* kOpenDisplays = "display.open";
inline constexpr const char* kOpenShortcuts = "shortcuts.open";
inline constexpr const char* kBack = "settings.back";
inline constexpr const char* kPickTheme = "theme.pick:";       // + its index in tokens().themes
inline constexpr const char* kPickDisplay = "display.pick:";   // + its index in SettingsView::displays
inline constexpr const char* kEditShortcut = "shortcut.edit:"; // + its index in commands()
inline constexpr const char* kToggleAutostart = "autostart.toggle";
inline constexpr const char* kCopyLogs = "logs.copy";
inline constexpr const char* kCentreIsland = "island.centre";
inline constexpr const char* kCheckUpdates = "updates.check";
// The choice pages' lists (Row::list), for Host::reveal.
inline constexpr const char* kThemeList = "themes";
inline constexpr const char* kDisplayList = "displays";
inline constexpr const char* kShortcutList = "shortcuts"; // the keys: more than fit on the card
} // namespace settings_action

inline constexpr const char* kSettingsPanelId = "system.settings"; // the widget's, and its card's
inline constexpr const char* kMainDisplayName = "Main display";    // the first display choice

enum class LogsNote { Ready, Copied, Failed }; // what the Copy logs button says

// What "Check for updates" says (decision 036): nothing yet, checking, or what it found.
struct UpdateNote {
    bool checking = false;
    std::optional<CheckOutcome> found;
    std::string version; // Available: the offer's
};

// Which page the card shows: the list of settings, or one setting's choices.
enum class SettingsPage { List, Themes, Displays, Shortcuts };

// Everything the card shows, so building it is one pure function.
struct SettingsView {
    SettingsPage page = SettingsPage::List;
    std::string theme;                  // the applied theme's name
    std::vector<std::string> displays;  // "Main display", then each monitor; empty: one monitor, no Display row
    std::size_t display = 0;            // the one the island is on
    std::optional<bool> startsWithWindows; // nothing: the app can't tell, no row
    LogsNote note = LogsNote::Ready;
    Bindings shortcuts = defaultBindings(); // what each command is bound to
    std::optional<Command> recording;       // waiting for new keys for this one
    std::string shortcutNote;               // under the shortcuts: "Press the new keys…", or why one wasn't changed
    std::string version;                    // the running build's; empty: no line for it
    std::optional<UpdateNote> update;       // nothing: the app can't check, no row
};

// The widget and its card.
Panel settingsPanel(const SettingsView& view);

// The theme page's list, top to bottom: dark themes under "Dark", then light
// ones under "Light" (Theme::light), each in tokens.json's order.
struct ThemeListRow {
    std::string heading;   // a group's heading, or empty for a theme
    std::size_t theme = 0; // a theme: its index in tokens().themes
};
std::vector<ThemeListRow> themeListRows();

} // namespace cup
