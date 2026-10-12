#pragma once
// Settings on Home (decision 022): a widget whose card picks the app theme,
// the display the island is on (decision 023), puts a moved island back in
// the centre (decision 026), and copies the log for a bug report. Picks are
// kept in the host's store (decision 028); recolouring, moving the island and
// the clipboard are the app's (handed in), so this stays pure and is tested on Linux.

#include "core/provider.h"

#include <optional>

#include <functional>
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
};

enum class LogsNote { Ready, Copied, Failed }; // what the Copy logs button says

// The widget and its card. `display`: the display row's label, or nothing
// to leave the row out (one monitor: nothing to choose).
Panel settingsPanel(const std::string& theme, const std::optional<std::string>& display, LogsNote note);

class SettingsProvider : public Provider {
public:
    explicit SettingsProvider(SettingsHooks hooks) : hooks_(std::move(hooks)) {}

    const char* id() const override { return "system.settings"; }
    void start(Host& host) override; // applies the saved theme, publishes the widget
    void stop() override;
    void onAction(const Action& action) override; // the card's theme arrows and Copy logs
    void onWake() override;                       // the note has shown for logsNoteMs: back to "Copy logs"

    // tokens.json was reloaded: its themes may have changed (one renamed, removed).
    void refresh();
    // Monitors were plugged, unplugged or rearranged: re-list them and put the
    // island back where it was picked (or on the main display if that's gone).
    void refreshDisplays();

    static constexpr const char* kPanelId = "system.settings";
    // Store keys (Host::load/save): the theme's name, the display's id ("" = the main display).
    // The app reads the display's too, at startup, to open on the right monitor.
    static constexpr const char* kThemeKey = "theme";
    static constexpr const char* kDisplayKey = "display";
    static constexpr const char* kPreviousTheme = "theme.previous";
    static constexpr const char* kNextTheme = "theme.next";
    static constexpr const char* kCopyLogs = "logs.copy";
    static constexpr const char* kCentreIsland = "island.centre";
    static constexpr const char* kPreviousDisplay = "display.previous";
    static constexpr const char* kNextDisplay = "display.next";
    static constexpr const char* kMainDisplayName = "Main display";

private:
    void pick(const std::string& theme); // apply it, show it
    void placeIsland();                   // apply the wanted display if connected, else the main one
    std::optional<std::string> displayLabel() const;
    void publish();

    SettingsHooks hooks_;
    Host* host_ = nullptr;
    std::string theme_;
    std::vector<DisplayChoice> displays_;
    std::string wantedDisplay_;  // the user's pick, kept while its monitor is unplugged
    std::string appliedDisplay_; // where the island is: wantedDisplay_ if connected, else "" (main)
    LogsNote note_ = LogsNote::Ready;
};

} // namespace cup
