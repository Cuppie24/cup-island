#pragma once
// The Settings window (decisions 030, 032, 090): built from a SettingsView by
// one pure function. Pages in a sidebar (Appearance, General, Updates) of
// grouped settings; a setting with choices (themes, displays, music colours)
// or the keyboard shortcuts opens a page within its page, with a back arrow.
// The widget on Home only opens it.

#include "core/island.h"
#include "core/window_content.h"
#include "core/shortcuts.h"
#include "core/tokens.h"
#include "providers/update/release.h"
#include "providers/update/update_provider.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cup {

// What the window's buttons send. A choice's action is its prefix and its index in the page's list.
namespace settings_action {
inline constexpr const char* kOpenWindow = "settings.window"; // the widget's tap: opens the window (decision 090)
inline constexpr const char* kSetTabRow = "tabrow.set";       // the Segmented: value 0 top, 1 bottom
inline constexpr const char* kOpenThemes = "theme.open";
inline constexpr const char* kOpenDisplays = "display.open";
inline constexpr const char* kOpenShortcuts = "shortcuts.open";
inline constexpr const char* kBack = "settings.back";
inline constexpr const char* kPickTheme = "theme.pick:";       // + its index in tokens().themes
inline constexpr const char* kPickDisplay = "display.pick:";   // + its index in SettingsView::displays
inline constexpr const char* kEditShortcut = "shortcut.edit:"; // + its index in commands()
inline constexpr const char* kToggleAutostart = "autostart.toggle";
inline constexpr const char* kToggleCoverColours = "covercolours.toggle"; // decision 052
inline constexpr const char* kOpenMusicColours = "musiccolours.open";      // decision 053
inline constexpr const char* kPickFade = "fade.pick:";                     // + 0 fast, 1 medium, 2 slow
inline constexpr const char* kCopyLogs = "logs.copy";
inline constexpr const char* kCentreIsland = "island.centre";
inline constexpr const char* kCheckUpdates = "updates.check";
inline constexpr const char* kInstallUpdate = "updates.install"; // the offer: install and restart (decision 037)
} // namespace settings_action

inline constexpr const char* kSettingsPanelId = "system.settings"; // the widget's
inline constexpr const char* kSettingsWindowId = "main";           // its window's (WindowSpec::id)
// The sidebar's pages (WindowPage::id).
inline constexpr const char* kAppearancePage = "appearance";
inline constexpr const char* kGeneralPage = "general";
inline constexpr const char* kUpdatesPage = "updates";
inline constexpr const char* kMainDisplayName = "Main display";    // the first display choice

enum class LogsNote { Ready, Copied, Failed }; // what the Copy logs button says

// What "Check for updates" says (decision 036): nothing yet, checking, or what it found.
struct UpdateNote {
    bool checking = false;
    std::optional<CheckOutcome> found;
    std::string version; // Available: the offer's
};

// A page within a sidebar page: none (its settings), or one setting's
// choices. Themes, Displays and MusicColours are within Appearance, Shortcuts within General.
enum class SettingsPage { List, Themes, Displays, Shortcuts, MusicColours };

// A setting added after people already had the app (decision 048): shown
// with a "New" tag until it's been seen, and a dot on its parent's row while
// it hasn't. Ids never change: they're what the store remembers.
struct SettingItem {
    std::string id;     // also its row's Row::id
    std::string parent; // the row whose page it's on; "" on the list itself
};

// Every announced setting, oldest first: add a line when adding a setting.
// Rows' ids: "theme", "musiccolours" (and "covercolours", "fade" on its page), "display",
// "tabrow", "centre", "autostart", "shortcuts", "logs", "updates".
std::vector<SettingItem> announcedSettings();

// Everything the window shows, so building it is one pure function.
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
    std::optional<UpdateStatus> offer;      // an update offered (by any check): an "Update and restart" row
    bool tabsOnTop = true;                  // the tab row's place (decision 048)
    std::optional<bool> coverColours;       // the island in the playing cover's colours (decision 052); nothing: no row
    std::optional<FadeSpeed> fadeSpeed;     // how fast colours fade (decision 053); nothing: no choice
    std::vector<SettingItem> announced;     // what can be new (announcedSettings)...
    std::set<std::string> unseen;           // ...and which of those haven't been seen yet: tagged "New"
};

// The announced settings on this page of the window, by their rows' ids: what the user sees of them now.
std::vector<std::string> shownSettings(const WindowContent& window, const std::string& page, const std::vector<SettingItem>& announced);

// The widget: a tap opens the window (Panel::tapAction), a dot while something's new in it.
Panel settingsPanel(const SettingsView& view);
// The window (decision 090).
WindowContent settingsWindow(const SettingsView& view);
// The sidebar page a setting's row is on, by its id ("theme" -> appearance).
std::string pageOfSetting(const std::string& id);

// The theme page's list, top to bottom: dark themes under "Dark", then light
// ones under "Light" (Theme::light), each in tokens.json's order.
struct ThemeListRow {
    std::string heading;   // a group's heading, or empty for a theme
    std::size_t theme = 0; // a theme: its index in tokens().themes
};
std::vector<ThemeListRow> themeListRows();

} // namespace cup
