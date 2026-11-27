#pragma once
// The Settings window (decisions 030, 032, 104): built from a SettingsView by
// one pure function. Pages in a sidebar (Appearance, General, Updates, Statistics) of
// grouped settings; a setting with choices (themes, music colours)
// or the keyboard shortcuts opens a page within its page, with a back arrow.
// Its button in the page row only opens it.

#include "core/island.h"
#include "core/window_content.h"
#include "core/shortcuts.h"
#include "core/tokens.h"
#include "providers/update/release.h"
#include "providers/update/update_provider.h"
#include "providers/system/window_theme.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cup {

// What the window's buttons send. A choice's action is its prefix and its index in the page's list.
namespace settings_action {
inline constexpr const char* kOpenWindow = "settings.window"; // its button's tap: opens the window (decision 104)
inline constexpr const char* kOpenThemes = "theme.open";
inline constexpr const char* kOpenShortcuts = "shortcuts.open";
inline constexpr const char* kPickTabRow = "tabrow.pick:";     // + 0 (top) or 1 (bottom): its row's Segments (decision 093)
inline constexpr const char* kBack = "settings.back";
inline constexpr const char* kPickTheme = "theme.pick:";       // + its index in tokens().themes
inline constexpr const char* kOpenWindowTheme = "windowtheme.open";      // decision 150
inline constexpr const char* kPickWindowThemeMode = "windowtheme.mode:"; // + 0 island, 1 system, 2 manual: its Segments
inline constexpr const char* kPickWindowTheme = "windowtheme.pick:";     // + its index in tokens().themes (Manual)
inline constexpr const char* kEditShortcut = "shortcut.edit:"; // + its index in commands()
inline constexpr const char* kResetShortcut = "shortcut.reset:"; // + its index in commands(): back to its default (decision 103)
inline constexpr const char* kResetShortcuts = "shortcuts.reset"; // every command back to its default (decision 103)
inline constexpr const char* kToggleAutostart = "autostart.toggle";
inline constexpr const char* kToggleCoverColours = "covercolours.toggle"; // decision 052
inline constexpr const char* kOpenMusicColours = "musiccolours.open";      // decision 053
inline constexpr const char* kPickFade = "fade.pick:";                     // + 0 fast, 1 medium, 2 slow
inline constexpr const char* kToggleGlass = "glass.toggle";                // decision 089
inline constexpr const char* kOpenGlass = "glass.open";                    // decision 096
inline constexpr const char* kPickGlass = "glass.pick:";                   // + 0 off, 1 frosted, 2 liquid (decision 165)
inline constexpr const char* kConfirmLiquid = "glass.liquid.confirm";      // ...liquid's question answered yes
inline constexpr const char* kPickTransparency = "transparency.pick:";     // + 0 low, 1 medium, 2 high
inline constexpr const char* kToggleGlassWindows = "glasswindows.toggle";  // decision 104
inline constexpr const char* kCopyLogs = "logs.copy";
inline constexpr const char* kCentreIsland = "island.centre";
inline constexpr const char* kCheckUpdates = "updates.check";
inline constexpr const char* kInstallUpdate = "updates.install"; // the offer: install and restart (decision 037)
inline constexpr const char* kWhatsNew = "updates.whatsnew";     // its release notes, in the browser (decision 152)
inline constexpr const char* kDownloadUpdate = "updates.download"; // a failed install: its installer, in the browser, to run by hand
inline constexpr const char* kToggleStatistic = "statistic.toggle:"; // + its index in SettingsView::statistics (decision 174)
} // namespace settings_action

inline constexpr const char* kSettingsPanelId = "system.settings"; // its button's panel
inline constexpr const char* kSettingsWindowId = "main";           // its window's (WindowSpec::id)
// The sidebar's pages (WindowPage::id).
inline constexpr const char* kAppearancePage = "appearance";
inline constexpr const char* kGeneralPage = "general";
inline constexpr const char* kUpdatesPage = "updates";
inline constexpr const char* kStatisticsPage = "statistics"; // what the pill shows of the statistics (decision 174)

enum class LogsNote { Ready, Copied, Failed }; // what the Copy logs button says

// What "Check for updates" says (decision 036): nothing yet, checking, or what it found.
struct UpdateNote {
    bool checking = false;
    std::optional<CheckOutcome> found;
    std::string version; // Available: the offer's
};

// A reading statistics can show in the pill (decision 174): a switch on the
// Statistics page. The readings and the rule for picking are the statistics
// provider's; Settings only lists them and passes a flip on.
struct SettingsChoice {
    std::string id;   // the provider's own
    std::string name; // "CPU load", a Bluetooth device's
    bool on = false;
    bool device = false; // a Bluetooth device's battery: under its own heading
    bool operator==(const SettingsChoice&) const = default;
};

// A page within a sidebar page: none (its settings), or one setting's
// choices. Themes, WindowTheme, MusicColours and Glass are within Appearance, Shortcuts within General.
enum class SettingsPage { List, Themes, WindowTheme, Shortcuts, MusicColours, Glass };

// A setting added after people already had the app (decision 048): shown
// with a "New" tag until it's been seen, and a dot on its parent's row while
// it hasn't. Ids never change: they're what the store remembers.
struct SettingItem {
    std::string id;     // also its row's Row::id
    std::string parent; // the row whose page it's on; "" on the list itself
};

// Every announced setting, oldest first: add a line when adding a setting.
// Rows' ids: "theme", "windowtheme", "glass" (and "transparency", "glasswindows" on its page), "musiccolours" (and "covercolours", "fade" on its page),
// "tabrow", "centre", "autostart", "shortcuts", "logs", "updates".
std::vector<SettingItem> announcedSettings();

// Everything the window shows, so building it is one pure function.
struct SettingsView {
    SettingsPage page = SettingsPage::List;
    std::string theme;                  // the applied theme's name
    std::optional<bool> startsWithWindows; // nothing: the app can't tell, no row
    LogsNote note = LogsNote::Ready;
    Bindings shortcuts = defaultBindings(); // what each command is bound to
    std::optional<Command> recording;       // waiting for new keys for this one
    std::string shortcutNote;               // under the shortcuts: "Press the new keys…", or why one wasn't changed
    std::string version;                    // the running build's; empty: no line for it
    std::optional<UpdateNote> update;       // nothing: the app can't check, no row
    std::optional<UpdateStatus> offer;      // an update offered (by any check): the updates row says "Update and restart" (decision 091)
    bool opensPages = false;                // the app can open a page in the browser: What's new, Download installer (decision 152)
    bool tabsOnTop = true;                  // the tab row's place (decision 048)
    std::optional<bool> coverColours;       // the island in the playing cover's colours (decision 052); nothing: no row
    std::optional<FadeSpeed> fadeSpeed;     // how fast colours fade (decision 053); nothing: no choice
    std::optional<bool> glass;              // the island as glass (decision 089); nothing: no row (Windows 10)
    std::optional<bool> liquidGlass;        // ...as liquid glass (decision 165); nothing: no such pick, Glass is a switch
    std::optional<Transparency> transparency; // how see-through (decision 096); nothing: no choice
    std::optional<bool> glassWindows;       // windows as glass too (decision 104); nothing: no row
    std::optional<WindowThemeMode> windowTheme; // where windows take their colours (decision 150); nothing: no row
    std::string windowThemeName;            // Manual: the theme picked for them
    std::vector<SettingsChoice> statistics; // the pill's readings (decision 174); empty: no Statistics page
    std::size_t statisticsMost = 0;         // ...and how many it shows at once, for the page's note
    std::vector<SettingItem> announced;     // what can be new (announcedSettings)...
    std::set<std::string> unseen;           // ...and which of those haven't been seen yet: tagged "New"
};

// What's new (decision 152): the offered update's release notes if one is
// offered, else this version's; empty for a dev build with nothing offered.
std::string whatsNewPage(const SettingsView& view);
// An update offered and still the user's to do (not installing): Settings' gear and its Updates page get an accent dot.
bool updateWaiting(const SettingsView& view);
// The announced settings this machine offers: those with a row on some page of
// the window, pages within pages included. One it lacks (Glass on Windows 10)
// can never be seen, so it never counts as new (decision 048).
std::set<std::string> offeredSettings(const SettingsView& view);

// The announced settings on this page of the window, by their rows' ids: what the user sees of them now.
std::vector<std::string> shownSettings(const WindowContent& window, const std::string& page, const std::vector<SettingItem>& announced);

// Its button: a tap opens the window (Panel::tapAction), a dot while something's new in it.
Panel settingsPanel(const SettingsView& view);
// The window (decision 104).
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
