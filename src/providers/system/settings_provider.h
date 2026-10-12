#pragma once
// Settings on Home (decisions 022, 030): a widget whose card is a list of
// settings: the app theme, the display the island is on (decision 023),
// Start with Windows, the keyboard shortcuts (decision 031), then Copy logs and Centre island (decision 026). Theme
// and Display open a page that lists every choice. Picks are kept in the
// host's store (decision 028); recolouring, moving the island, the Run key
// and the clipboard are the app's (handed in), so this stays pure and is tested on Linux.

#include "core/provider.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// A connected monitor the island can be put on.
struct DisplayChoice {
    std::string id;   // stable across reboots and replugging (its device path)
    std::string name; // what the card shows, e.g. "DELL U2720Q"
    bool operator==(const DisplayChoice&) const = default;
};

// What the card needs from the app.
struct SettingsHooks {
    std::function<void(const std::string&)> applyTheme;  // recolours the island (tokens.json's "themes")
    std::function<bool()> copyLogs;                      // true: the log is on the clipboard
    std::function<std::vector<DisplayChoice>()> listDisplays; // connected monitors, in Windows' order
    std::function<void(const std::string&)> applyDisplay; // moves the island; "" = the main display
    std::function<void()> centreIsland;                  // back to the middle of the top edge (decision 026)
    std::function<bool()> startsWithWindows;             // read each time: Task Manager may have changed it
    std::function<void(bool)> setStartsWithWindows;
};

enum class LogsNote { Ready, Copied, Failed }; // what the Copy logs button says

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

class SettingsProvider : public Provider {
public:
    explicit SettingsProvider(SettingsHooks hooks) : hooks_(std::move(hooks)) {}

    const char* id() const override { return "system.settings"; }
    void start(Host& host) override; // applies the saved theme and display, Start with Windows the first time
    void stop() override;
    void onAction(const Action& action) override; // the card's rows, choices and buttons
    void onWake() override;                       // the note has shown for logsNoteMs: back to "Copy logs"

    // tokens.json was reloaded: its themes may have changed (one renamed, removed).
    void refresh();
    // Monitors were plugged, unplugged or rearranged: re-list them and put the
    // island back where it was picked (or on the main display if that's gone).
    void refreshDisplays();

    static constexpr const char* kPanelId = "system.settings";
    // Store keys (Host::load/save): the theme's name, the display's id ("" = the main display),
    // and Start with Windows as last set ("on"/"off"; "" = never: it starts on, decision 030).
    // The app reads the display's too, at startup, to open on the right monitor.
    static constexpr const char* kThemeKey = "theme";
    static constexpr const char* kDisplayKey = "display";
    static constexpr const char* kAutostartKey = "autostart";
    // Actions. A choice's action is its prefix and its index in the page's list.
    static constexpr const char* kOpenThemes = "theme.open";
    static constexpr const char* kOpenDisplays = "display.open";
    static constexpr const char* kOpenShortcuts = "shortcuts.open"; // a page listing the keyboard shortcuts (decision 031)
    static constexpr const char* kBack = "settings.back";
    static constexpr const char* kPickTheme = "theme.pick:";
    static constexpr const char* kPickDisplay = "display.pick:";
    static constexpr const char* kToggleAutostart = "autostart.toggle";
    static constexpr const char* kCopyLogs = "logs.copy";
    static constexpr const char* kCentreIsland = "island.centre";
    static constexpr const char* kMainDisplayName = "Main display";
    // The choice pages' lists (Row::list), for Host::reveal.
    static constexpr const char* kThemeList = "themes";
    static constexpr const char* kDisplayList = "displays";

private:
    void pick(const std::string& theme); // apply it, show it
    void pickDisplay(const std::string& id);
    void placeIsland();                   // apply the wanted display if connected, else the main one
    void open(SettingsPage page);         // a choice page, scrolled to what's picked
    SettingsView view() const;
    void publish();

    SettingsHooks hooks_;
    Host* host_ = nullptr;
    SettingsPage page_ = SettingsPage::List;
    std::string theme_;
    std::vector<DisplayChoice> displays_;
    std::string wantedDisplay_;  // the user's pick, kept while its monitor is unplugged
    std::string appliedDisplay_; // where the island is: wantedDisplay_ if connected, else "" (main)
    LogsNote note_ = LogsNote::Ready;
};

} // namespace cup
