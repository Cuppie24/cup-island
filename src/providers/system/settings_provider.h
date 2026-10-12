#pragma once
// Settings on Home (decision 022): a widget whose card picks the app theme
// and copies the log for a bug report. Saving the pick, recolouring the
// island and the clipboard are the app's (handed in), so this stays pure
// and is tested on Linux.

#include "core/provider.h"

#include <functional>
#include <string>

namespace cup {

// What the card needs from the app.
struct SettingsHooks {
    std::function<std::string()> loadTheme;              // the saved theme's name, "" if none
    std::function<void(const std::string&)> saveTheme;
    std::function<void(const std::string&)> applyTheme;  // recolours the island (tokens.json's "themes")
    std::function<bool()> copyLogs;                      // true: the log is on the clipboard
};

enum class LogsNote { Ready, Copied, Failed }; // what the Copy logs button says

// The widget and its card, for the theme shown and the button's note.
Panel settingsPanel(const std::string& theme, LogsNote note);

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

    static constexpr const char* kPanelId = "system.settings";
    static constexpr const char* kPreviousTheme = "theme.previous";
    static constexpr const char* kNextTheme = "theme.next";
    static constexpr const char* kCopyLogs = "logs.copy";

private:
    void pick(const std::string& theme); // apply it, show it
    void publish();

    SettingsHooks hooks_;
    Host* host_ = nullptr;
    std::string theme_;
    LogsNote note_ = LogsNote::Ready;
};

} // namespace cup
