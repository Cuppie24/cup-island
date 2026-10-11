#pragma once
// "Copy logs" on Home: a tap puts the app's recent log on the clipboard, so
// a bug report is one paste. Copying is the platform's (passed in); this
// only decides what the tile says. Pure: tested on Linux.

#include "core/provider.h"

#include <functional>

namespace cup {

enum class LogsTile { Ready, Copied, Failed };

// The tile for each state: a tap action, no card.
Panel logsPanel(LogsTile state);

class LogsProvider : public Provider {
public:
    using CopyLogs = std::function<bool()>; // true: the log is on the clipboard
    explicit LogsProvider(CopyLogs copy) : copy_(std::move(copy)) {}

    const char* id() const override { return "system.logs"; }
    void start(Host& host) override; // publishes the tile
    void stop() override;
    void onAction(const Action& action) override; // the tap: copy, say how it went
    void onWake() override;                       // the note has shown for logsNoteMs: back to "Logs"

    static constexpr const char* kPanelId = "system.logs";
    static constexpr const char* kCopy = "copy"; // the tile's tap action

private:
    CopyLogs copy_;
    Host* host_ = nullptr;
};

} // namespace cup
