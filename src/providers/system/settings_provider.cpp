#include "providers/system/settings_provider.h"

#include "core/tokens.h"

#include <algorithm>
#include <sstream>

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

// The index after a choice's prefix ("theme.pick:3" -> 3), if the action is one.
std::optional<std::size_t> choiceIndex(const std::string& actionId, const char* prefix)
{
    std::string p(prefix);
    if (actionId.size() <= p.size() || actionId.compare(0, p.size(), p) != 0) return std::nullopt;
    std::size_t at = 0;
    for (char c : actionId.substr(p.size())) {
        if (c < '0' || c > '9') return std::nullopt;
        at = at * 10 + static_cast<std::size_t>(c - '0');
    }
    return at;
}

} // namespace

void SettingsProvider::start(Host& host)
{
    host_ = &host;
    note_ = LogsNote::Ready;
    page_ = SettingsPage::List;
    visible_ = false;
    seeing_.clear();
    loadSeen(); // first: a first run is told by nothing saved yet, and the lines below save
    // Start with Windows is on until the user turns it off (decision 030):
    // set once, the first time, so a later "off" (here or in Task Manager) sticks.
    if (hooks_.setStartsWithWindows && trimmed(host_->load(kAutostartKey)).empty()) {
        hooks_.setStartsWithWindows(true);
        host_->save(kAutostartKey, "on");
    }
    // Shortcuts (decision 032): the saved keys, registered system-wide. One
    // another app already has stays as saved, and the page says so.
    shortcuts_ = loadBindings(host_->load(kShortcutsKey));
    recording_.reset();
    shortcutNote_.clear();
    if (hotkeysActive_) bindAll();
    displays_ = hooks_.listDisplays ? hooks_.listDisplays() : std::vector<DisplayChoice>{};
    wantedDisplay_ = trimmed(host_->load(kDisplayKey));
    placeIsland();
    tabsOnTop_ = trimmed(host_->load(kTabRowKey)) != "bottom";
    if (hooks_.placeTabRow) hooks_.placeTabRow(tabsOnTop_);
    // Off unless turned on (decision 055).
    coverColours_ = trimmed(host_->load(kCoverColoursKey)) == "on";
    if (hooks_.setCoverColours) hooks_.setCoverColours(coverColours_);
    coverGradient_ = trimmed(host_->load(kCoverGradientKey)) == "on";
    if (hooks_.setCoverGradient) hooks_.setCoverGradient(coverGradient_);
    coverBackdrop_ = trimmed(host_->load(kCoverBackdropKey)) == "on";
    if (hooks_.setCoverBackdrop) hooks_.setCoverBackdrop(coverBackdrop_);
    std::string fade = trimmed(host_->load(kFadeKey));
    fadeSpeed_ = fade == "fast" ? FadeSpeed::Fast : fade == "slow" ? FadeSpeed::Slow : FadeSpeed::Medium;
    if (hooks_.setFadeSpeed) hooks_.setFadeSpeed(fadeSpeed_);
    pick(known(trimmed(host_->load(kThemeKey))));
}

void SettingsProvider::loadSeen()
{
    seen_.clear();
    std::istringstream lines(host_->load(kSeenKey));
    std::string line;
    if (std::getline(lines, line) && trimmed(line) == "seen1") {
        while (std::getline(lines, line))
            if (!trimmed(line).empty()) seen_.insert(trimmed(line));
        return;
    }
    // Nothing saved by this or an older version: a fresh install, where every
    // setting is as new as the rest (none tagged), or an update from before
    // tags existed, which kept other settings (all announced ones are new).
    bool fresh = true;
    for (const char* key : { kAutostartKey, kThemeKey, kDisplayKey, kShortcutsKey, kTabRowKey, kCoverColoursKey, kFadeKey,
                             kCoverGradientKey, kCoverBackdropKey })
        if (!trimmed(host_->load(key)).empty()) fresh = false;
    if (!fresh) return;
    for (const SettingItem& s : hooks_.announced) seen_.insert(s.id);
    std::string saved = "seen1\n";
    for (const std::string& id : seen_) saved += id + "\n";
    host_->save(kSeenKey, saved);
}

void SettingsProvider::placeTabRow(bool top)
{
    tabsOnTop_ = top;
    host_->save(kTabRowKey, top ? "top" : "bottom");
    if (hooks_.placeTabRow) hooks_.placeTabRow(top);
    publish();
}

void SettingsProvider::pickFade(FadeSpeed speed)
{
    fadeSpeed_ = speed;
    host_->save(kFadeKey, speed == FadeSpeed::Fast ? "fast" : speed == FadeSpeed::Slow ? "slow" : "medium");
    if (hooks_.setFadeSpeed) hooks_.setFadeSpeed(speed);
    publish();
}

void SettingsProvider::onCardShown(const std::string& activityId)
{
    if (!host_ || activityId != kPanelId) return;
    visible_ = true;
    publish(); // counts what's on it as being seen
}

void SettingsProvider::stop()
{
    if (!host_) return;
    stopRecording();
    host_->wakeAt(std::nullopt);
    host_->removePanel(kPanelId);
    host_ = nullptr;
}

void SettingsProvider::onAction(const Action& action)
{
    if (!host_ || action.activityId != kPanelId) return;
    const std::string& id = action.actionId;
    const auto& themes = tokens().themes;
    if (id == settings_action::kOpenThemes && !themes.empty()) open(SettingsPage::Themes);
    else if (id == settings_action::kOpenDisplays && displays_.size() >= 2) open(SettingsPage::Displays);
    else if (id == settings_action::kOpenShortcuts) {
        page_ = SettingsPage::Shortcuts;
        publish();
    }
    else if (id == settings_action::kOpenMusicColours && hooks_.setCoverColours) {
        page_ = SettingsPage::MusicColours;
        publish();
    } else if (auto at = choiceIndex(id, settings_action::kPickFade)) {
        // The page stays, like the tab row's: the next recolour shows the new speed.
        if (*at <= 2 && hooks_.setFadeSpeed) pickFade(*at == 0 ? FadeSpeed::Fast : *at == 2 ? FadeSpeed::Slow : FadeSpeed::Medium);
    }
    else if (id == settings_action::kOpenTabRow) {
        page_ = SettingsPage::TabRow;
        publish();
    } else if (auto at = choiceIndex(id, settings_action::kPickTabRow)) {
        if (*at <= 1) placeTabRow(*at == 0); // the page stays: the card behind it shows where the row went
    }
    else if (id == settings_action::kBack) {
        stopRecording();
        shortcutNote_.clear();
        page_ = SettingsPage::List;
        publish();
    } else if (auto at = choiceIndex(id, settings_action::kPickTheme)) {
        // The page stays: the island, recoloured at once, is the preview, so
        // trying one theme after another is a click each (decision 030).
        if (*at < themes.size()) {
            pick(themes[*at].name);
            host_->save(kThemeKey, theme_);
        }
    } else if (auto at = choiceIndex(id, settings_action::kPickDisplay)) {
        if (*at <= displays_.size()) pickDisplay(*at == 0 ? std::string() : displays_[*at - 1].id); // 0: "Main display"
    } else if (auto at = choiceIndex(id, settings_action::kEditShortcut)) {
        if (*at < commands().size() && hooks_.captureKeys) {
            recording_ = commands()[*at].command;
            shortcutNote_ = "Press the new keys, or Esc to cancel";
            hooks_.captureKeys(true);
            publish();
        }
    } else if (id == settings_action::kToggleAutostart && hooks_.startsWithWindows && hooks_.setStartsWithWindows) {
        bool on = !hooks_.startsWithWindows();
        hooks_.setStartsWithWindows(on);
        host_->save(kAutostartKey, on ? "on" : "off");
        publish();
    } else if (id == settings_action::kToggleCoverColours && hooks_.setCoverColours) {
        coverColours_ = !coverColours_;
        hooks_.setCoverColours(coverColours_);
        host_->save(kCoverColoursKey, coverColours_ ? "on" : "off");
        publish();
    } else if (id == settings_action::kToggleCoverGradient && hooks_.setCoverGradient && coverColours_) {
        coverGradient_ = !coverGradient_;
        hooks_.setCoverGradient(coverGradient_);
        host_->save(kCoverGradientKey, coverGradient_ ? "on" : "off");
        publish();
    } else if (id == settings_action::kToggleCoverBackdrop && hooks_.setCoverBackdrop) {
        coverBackdrop_ = !coverBackdrop_;
        hooks_.setCoverBackdrop(coverBackdrop_);
        host_->save(kCoverBackdropKey, coverBackdrop_ ? "on" : "off");
        publish();
    } else if (id == settings_action::kInstallUpdate) {
        if (hooks_.installUpdate && offer_ && offer_->stage != UpdateStage::Installing) hooks_.installUpdate();
    } else if (id == settings_action::kCheckUpdates) {
        if (!hooks_.checkForUpdates || updateNote_.checking) return;
        updateNote_ = { .checking = true };
        publish();
        hooks_.checkForUpdates([this](CheckOutcome found, const std::string& version) {
            if (!host_) return; // stopped meanwhile
            updateNote_ = { .found = found, .version = version };
            publish();
        });
    } else if (id == settings_action::kCentreIsland) {
        if (hooks_.centreIsland) hooks_.centreIsland();
    } else if (id == settings_action::kCopyLogs) {
        note_ = hooks_.copyLogs && hooks_.copyLogs() ? LogsNote::Copied : LogsNote::Failed;
        publish();
        host_->wakeAt(host_->now() + tokens().logsNote); // a second click restarts the note
    }
}

void SettingsProvider::pickDisplay(const std::string& id)
{
    wantedDisplay_ = id;
    host_->save(kDisplayKey, wantedDisplay_);
    page_ = SettingsPage::List;
    placeIsland();
    publish();
}

void SettingsProvider::open(SettingsPage page)
{
    page_ = page;
    publish();
    // Scrolled to what's picked: the list may be longer than its window.
    std::size_t row = 0;
    if (page == SettingsPage::Themes) {
        auto rows = themeListRows();
        std::size_t picked = indexOf(theme_);
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].heading.empty() && rows[i].theme == picked) row = i;
        host_->reveal(kPanelId, settings_action::kThemeList, row);
    } else {
        host_->reveal(kPanelId, settings_action::kDisplayList, view().display);
    }
}

void SettingsProvider::keysPressed(const std::optional<Chord>& chord)
{
    if (!host_ || !recording_) return;
    Command command = *recording_;
    stopRecording();
    shortcutNote_.clear();
    Chord was = chordOf(shortcuts_, command);
    if (chord && *chord != was) {
        // Abort on any conflict: the old keys stay, and the note says why.
        if (std::string problem = bindingProblem(shortcuts_, command, *chord); !problem.empty()) {
            shortcutNote_ = problem;
        } else if (hooks_.bindHotkey && !hooks_.bindHotkey(command, *chord)) {
            hooks_.bindHotkey(command, was); // back to the old keys
            shortcutNote_ = chordText(*chord) + " is taken by another app";
        } else {
            for (Binding& b : shortcuts_)
                if (b.command == command) b.chord = *chord;
            host_->save(kShortcutsKey, saveBindings(shortcuts_));
        }
    }
    publish();
}

void SettingsProvider::bindAll()
{
    if (!hooks_.bindHotkey) return;
    shortcutNote_.clear(); // a "taken" from before is told again if it still is
    for (const Binding& b : shortcuts_)
        if (!hooks_.bindHotkey(b.command, b.chord)) shortcutNote_ = chordText(b.chord) + " is taken by another app";
}

void SettingsProvider::setHotkeysActive(bool on)
{
    if (on == hotkeysActive_) return;
    hotkeysActive_ = on;
    if (!host_) return; // start() registers them, or not
    if (!on) {
        stopRecording(); // the island is going: so is the page waiting for keys
        if (hooks_.unbindHotkeys) hooks_.unbindHotkeys();
        return;
    }
    bindAll();
    publish();
}

void SettingsProvider::stopRecording()
{
    if (!recording_) return;
    recording_.reset();
    if (hooks_.captureKeys) hooks_.captureKeys(false);
}

// The card closed: next time it opens at the list of settings, not the page
// it was left on, and a shortcut waiting for keys stops waiting.
void SettingsProvider::onCardClosed(const std::string& activityId)
{
    if (!host_ || activityId != kPanelId) return;
    visible_ = false;
    // What was shown this visit is seen now: next time it's not "New"
    // (decision 048). What wasn't shown (a page not opened) still is.
    bool newlySeen = false;
    for (const std::string& id : seeing_) newlySeen = seen_.insert(id).second || newlySeen;
    seeing_.clear();
    if (newlySeen) {
        std::string saved = "seen1\n";
        for (const std::string& id : seen_) saved += id + "\n";
        host_->save(kSeenKey, saved);
    }
    bool noted = updateNote_.found.has_value();
    if (!updateNote_.checking) updateNote_ = {}; // reopened: "Check for updates" says nothing until clicked again
    if (page_ == SettingsPage::List && !recording_) {
        if (noted || newlySeen) publish();
        return;
    }
    stopRecording();
    shortcutNote_.clear();
    page_ = SettingsPage::List;
    publish();
}

void SettingsProvider::updateStatusChanged(const std::optional<UpdateStatus>& offer)
{
    if (offer == offer_) return;
    offer_ = offer;
    if (host_) publish();
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
    if (page_ == SettingsPage::Displays && displays_.size() < 2) page_ = SettingsPage::List; // nothing left to choose
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

SettingsView SettingsProvider::view() const
{
    SettingsView v{ .page = page_, .theme = theme_, .note = note_, .shortcuts = shortcuts_, .recording = recording_,
                    .shortcutNote = shortcutNote_, .version = hooks_.version, .tabsOnTop = tabsOnTop_, .announced = hooks_.announced };
    for (const SettingItem& s : hooks_.announced)
        if (!seen_.count(s.id)) v.unseen.insert(s.id); // still "New" while being seen: until the card closes
    if (hooks_.checkForUpdates) v.update = updateNote_;
    v.offer = offer_;
    if (displays_.size() >= 2) { // one monitor: nothing to choose
        v.displays.push_back(kMainDisplayName);
        for (std::size_t i = 0; i < displays_.size(); ++i) {
            v.displays.push_back(displays_[i].name);
            if (displays_[i].id == appliedDisplay_) v.display = i + 1;
        }
    }
    if (hooks_.startsWithWindows) v.startsWithWindows = hooks_.startsWithWindows();
    if (hooks_.setCoverColours) v.coverColours = coverColours_;
    if (hooks_.setCoverColours && hooks_.setFadeSpeed) v.fadeSpeed = fadeSpeed_; // on Music colours' page
    if (hooks_.setCoverColours && hooks_.setCoverGradient) v.coverGradient = coverGradient_;
    if (hooks_.setCoverColours && hooks_.setCoverBackdrop) v.coverBackdrop = coverBackdrop_;
    return v;
}

void SettingsProvider::pick(const std::string& theme)
{
    theme_ = theme;
    if (hooks_.applyTheme) hooks_.applyTheme(theme_);
    publish();
}

void SettingsProvider::publish()
{
    Panel panel = settingsPanel(view());
    if (visible_) // on screen: what it shows of the announced settings is being seen
        for (const std::string& id : shownSettings(panel, hooks_.announced)) seeing_.insert(id);
    host_->publishPanel(std::move(panel));
}

} // namespace cup
