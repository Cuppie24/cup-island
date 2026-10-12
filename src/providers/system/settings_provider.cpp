#include "providers/system/settings_provider.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// The theme's place in tokens.json's list; an unknown name counts as the first (the default).
std::size_t indexOf(const std::string& theme)
{
    const auto& themes = tokens().themes;
    auto it = std::find_if(themes.begin(), themes.end(), [&](const Theme& t) { return t.name == theme; });
    return it == themes.end() ? 0 : static_cast<std::size_t>(it - themes.begin());
}

std::string trimmed(std::string text)
{
    text.erase(text.find_last_not_of(" \t\r\n") + 1); // a hand-edited file's newline
    return text;
}

// A name tokens.json still has: a saved one that was renamed or removed falls back to the default.
std::string known(const std::string& theme)
{
    const auto& themes = tokens().themes;
    return themes.empty() ? std::string() : themes[indexOf(theme)].name;
}

} // namespace

// The widget: a gear, like Windows' own Settings. The card, centred: the
// theme between two arrows (the island recolours at once, so the island is
// the preview), then Copy logs, which says how it went for a moment.
Panel settingsPanel(const std::string& theme, const std::optional<std::string>& display, LogsNote note)
{
    // A setting: its caption, then its value between ‹ › arrows that cycle.
    Slot card;
    auto picker = [&](const char* caption, const std::string& value, const char* previous, const char* next) {
        card.push_back(row({ Text{ .value = caption, .style = TextStyle::Caption, .centred = true } }));
        card.push_back(Row{ .items = { Text{ .value = value, .style = TextStyle::Title, .centred = true } },
                            .leading = { Button{ Glyph::ChevronLeft, previous } },
                            .trailing = { Button{ Glyph::ChevronRight, next } } });
    };
    if (!tokens().themes.empty()) picker("Theme", theme, SettingsProvider::kPreviousTheme, SettingsProvider::kNextTheme);
    if (display) picker("Display", *display, SettingsProvider::kPreviousDisplay, SettingsProvider::kNextDisplay);
    const char* logs = note == LogsNote::Copied ? "Logs copied" : note == LogsNote::Failed ? "Couldn't copy logs" : "Copy logs";
    card.push_back(row({ Button{ .glyph = Glyph::Copy, .actionId = SettingsProvider::kCopyLogs, .label = logs } }, Align::Center));
    Content content{
        .label = "Settings",
        .card = CardLayout::Centred,
        .expandedCenter = card,
        .tile = { row({ Icon{ Glyph::Settings } }), row({ Text{ "Settings" } }) },
    };
    return { .id = SettingsProvider::kPanelId, .content = content };
}

void SettingsProvider::start(Host& host)
{
    host_ = &host;
    note_ = LogsNote::Ready;
    displays_ = hooks_.listDisplays ? hooks_.listDisplays() : std::vector<DisplayChoice>{};
    wantedDisplay_ = trimmed(hooks_.loadDisplay ? hooks_.loadDisplay() : std::string());
    placeIsland();
    pick(known(trimmed(hooks_.loadTheme ? hooks_.loadTheme() : std::string())));
}

void SettingsProvider::stop()
{
    if (!host_) return;
    host_->wakeAt(std::nullopt);
    host_->removePanel(kPanelId);
    host_ = nullptr;
}

void SettingsProvider::onAction(const Action& action)
{
    if (!host_ || action.activityId != kPanelId) return;
    const auto& themes = tokens().themes;
    if ((action.actionId == kNextTheme || action.actionId == kPreviousTheme) && !themes.empty()) {
        std::size_t n = themes.size(), at = indexOf(theme_);
        std::size_t next = action.actionId == kNextTheme ? (at + 1) % n : (at + n - 1) % n; // wraps around
        pick(themes[next].name);
        if (hooks_.saveTheme) hooks_.saveTheme(theme_);
    } else if ((action.actionId == kNextDisplay || action.actionId == kPreviousDisplay) && !displays_.empty()) {
        // "Main display" first, then each monitor; wraps around like the themes.
        std::vector<std::string> ids{ "" };
        for (const auto& d : displays_) ids.push_back(d.id);
        std::size_t n = ids.size();
        std::size_t at = static_cast<std::size_t>(std::find(ids.begin(), ids.end(), appliedDisplay_) - ids.begin()) % n;
        wantedDisplay_ = ids[action.actionId == kNextDisplay ? (at + 1) % n : (at + n - 1) % n];
        if (hooks_.saveDisplay) hooks_.saveDisplay(wantedDisplay_);
        placeIsland();
        publish();
    } else if (action.actionId == kCopyLogs) {
        note_ = hooks_.copyLogs && hooks_.copyLogs() ? LogsNote::Copied : LogsNote::Failed;
        publish();
        host_->wakeAt(host_->now() + tokens().logsNote); // a second click restarts the note
    }
}

void SettingsProvider::onWake()
{
    if (!host_) return;
    note_ = LogsNote::Ready;
    publish();
}

void SettingsProvider::refresh()
{
    if (host_) pick(known(theme_));
}

void SettingsProvider::refreshDisplays()
{
    if (!host_) return;
    displays_ = hooks_.listDisplays ? hooks_.listDisplays() : std::vector<DisplayChoice>{};
    placeIsland();
    publish();
}

void SettingsProvider::placeIsland()
{
    bool connected = std::any_of(displays_.begin(), displays_.end(), [&](const DisplayChoice& d) { return d.id == wantedDisplay_; });
    appliedDisplay_ = connected ? wantedDisplay_ : std::string();
    // Always: the same monitor may have a new size or place (a resolution change).
    if (hooks_.applyDisplay) hooks_.applyDisplay(appliedDisplay_);
}

std::optional<std::string> SettingsProvider::displayLabel() const
{
    if (displays_.size() < 2) return std::nullopt; // one monitor: nothing to choose
    for (const auto& d : displays_)
        if (d.id == appliedDisplay_) return d.name;
    return std::string(kMainDisplayName);
}

void SettingsProvider::pick(const std::string& theme)
{
    theme_ = theme;
    if (hooks_.applyTheme) hooks_.applyTheme(theme_);
    publish();
}

void SettingsProvider::publish() { host_->publishPanel(settingsPanel(theme_, displayLabel(), note_)); }

} // namespace cup
