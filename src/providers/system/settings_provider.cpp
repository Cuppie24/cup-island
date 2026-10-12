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
Panel settingsPanel(const std::string& theme, LogsNote note)
{
    Slot card;
    if (!tokens().themes.empty()) {
        card.push_back(row({ Text{ .value = "Theme", .style = TextStyle::Caption, .centred = true } }));
        card.push_back(Row{ .items = { Text{ .value = theme, .style = TextStyle::Title, .centred = true } },
                            .leading = { Button{ Glyph::ChevronLeft, SettingsProvider::kPreviousTheme } },
                            .trailing = { Button{ Glyph::ChevronRight, SettingsProvider::kNextTheme } } });
    }
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
    std::string saved = hooks_.loadTheme ? hooks_.loadTheme() : std::string();
    saved.erase(saved.find_last_not_of(" \t\r\n") + 1); // a hand-edited file's newline
    pick(known(saved));
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

void SettingsProvider::pick(const std::string& theme)
{
    theme_ = theme;
    if (hooks_.applyTheme) hooks_.applyTheme(theme_);
    publish();
}

void SettingsProvider::publish() { host_->publishPanel(settingsPanel(theme_, note_)); }

} // namespace cup
