#pragma once
// Settings on Home (decisions 022, 030): what the card's actions do. The
// app theme, colours from music (decision 052), the display (decision 023), Start with Windows, the keyboard
// shortcuts (decisions 031, 032), Copy logs and Centre island (decision 026).
// Picks are kept in the host's store (decision 028); recolouring, moving the
// island, the Run key, hotkeys and the clipboard are the app's (handed in),
// so this stays pure and is tested on Linux. The card itself: settings_card.h.

#include "core/provider.h"
#include "core/shortcuts.h"
#include "providers/system/settings_card.h"

#include <functional>
#include <optional>
#include <set>
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
    // System-wide shortcuts (decision 032): registers `chord` for `command`
    // in place of what it had; false if Windows refused (another app has it).
    std::function<bool(Command, const Chord&)> bindHotkey;
    std::function<void()> unbindHotkeys; // every command's, until bound again
    // On: the app sends the next key pressed on the island to keysPressed()
    // instead of acting on it. Off again once one arrives, or on cancel.
    std::function<void(bool)> captureKeys;
    std::string version; // the running build's, shown under the list (decision 036); empty: no line
    // "Check for updates" (decision 036): checks now, then calls back on the
    // UI thread with what it found (Available: the offer's version). Empty: no row.
    std::function<void(std::function<void(CheckOutcome, const std::string&)>)> checkForUpdates;
    std::function<void()> installUpdate; // the offer, as Home's Update card installs it (decision 037)
    std::function<void(bool top)> placeTabRow; // the tab row along the card's top or bottom (decision 048)
    // The island in the playing cover's colours, or always the theme's (decision 052). Empty: no row.
    std::function<void(bool on)> setCoverColours;
    // What can be tagged "New" (decision 048): the shipped list, unless a test brings its own.
    std::vector<SettingItem> announced = announcedSettings();
};

class SettingsProvider : public Provider {
public:
    explicit SettingsProvider(SettingsHooks hooks) : hooks_(std::move(hooks)) {}

    const char* id() const override { return "system.settings"; }
    void start(Host& host) override; // applies the saved theme and display, Start with Windows the first time
    void stop() override;
    void onAction(const Action& action) override; // the card's rows, choices and buttons
    void onWake() override;                       // the note has shown for logsNoteMs: back to "Copy logs"
    void onCardClosed(const std::string& activityId) override; // reopened, it starts at the list; what was seen stays seen
    void onCardShown(const std::string& activityId) override;  // what's on it now is being seen (decision 048)

    // The update offer changed (UpdateProvider::onStatus): one found, its
    // install under way or failed, or none. Settings shows it under "Check for updates".
    void updateStatusChanged(const std::optional<UpdateStatus>& offer);

    // tokens.json was reloaded: its themes may have changed (one renamed, removed).
    void refresh();
    // A key pressed while captureKeys is on: the new keys for the shortcut
    // being changed, or nothing (Esc) to leave it. A chord that can't be bound
    // (no Ctrl/Alt/Win, another command's, taken by another app) changes nothing.
    void keysPressed(const std::optional<Chord>& chord);
    // While recording, a command's own registered keys were pressed (they
    // reach the app as that hotkey, not as keys): the same as pressing them.
    void shortcutPressed(Command command) { keysPressed(chordOf(shortcuts_, command)); }
    // Off while a fullscreen app hides the island (decision 032): the
    // shortcuts are let go, so their keys reach that app; on again after.
    // Before start(), it only says whether start() registers them.
    void setHotkeysActive(bool on);
    // Monitors were plugged, unplugged or rearranged: re-list them and put the
    // island back where it was picked (or on the main display if that's gone).
    void refreshDisplays();

    static constexpr const char* kPanelId = kSettingsPanelId;
    // Store keys (Host::load/save): the theme's name, the display's id ("" = the main display),
    // and Start with Windows as last set ("on"/"off"; "" = never: it starts on, decision 030).
    // The app reads the display's too, at startup, to open on the right monitor.
    static constexpr const char* kThemeKey = "theme";
    static constexpr const char* kDisplayKey = "display";
    static constexpr const char* kAutostartKey = "autostart";
    static constexpr const char* kShortcutsKey = "shortcuts"; // saveBindings' text
    static constexpr const char* kTabRowKey = "tabrow";       // "top" or "bottom" ("": top)
    static constexpr const char* kCoverColoursKey = "covercolours"; // "on" or "off" ("": on, decision 052)
    static constexpr const char* kSeenKey = "seen";           // "seen1", then the announced settings seen, one id per line

private:
    void pick(const std::string& theme); // apply it, show it
    void pickDisplay(const std::string& id);
    void placeIsland();                   // apply the wanted display if connected, else the main one
    void open(SettingsPage page);         // a choice page, scrolled to what's picked
    SettingsView view() const;
    void publish();
    void stopRecording();
    void bindAll(); // registers every shortcut; one another app has stays, the page says so
    void loadSeen();  // what's been seen; a first run has seen everything (nothing is new on a fresh install)
    void placeTabRow(bool top);

    SettingsHooks hooks_;
    Host* host_ = nullptr;
    SettingsPage page_ = SettingsPage::List;
    std::string theme_;
    std::vector<DisplayChoice> displays_;
    std::string wantedDisplay_;  // the user's pick, kept while its monitor is unplugged
    std::string appliedDisplay_; // where the island is: wantedDisplay_ if connected, else "" (main)
    LogsNote note_ = LogsNote::Ready;
    std::optional<UpdateStatus> offer_;
    Bindings shortcuts_;
    std::optional<Command> recording_;
    bool hotkeysActive_ = true;
    std::string shortcutNote_;
    UpdateNote updateNote_;
    bool tabsOnTop_ = true;
    bool coverColours_ = true;
    std::set<std::string> seen_;     // announced settings seen in earlier visits (decision 048)
    std::set<std::string> seeing_;   // ...and shown during this one: seen once the card closes
    bool visible_ = false;           // the card is on screen
};

} // namespace cup
