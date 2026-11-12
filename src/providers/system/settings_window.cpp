#include "providers/system/settings_window.h"

#include "core/shortcuts.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

namespace P = settings_action;

// A setting with choices (decision 030): its name, what it's set to under
// it, and a chevron: a click anywhere on the row opens its choices.
Row settingRow(const char* name, const std::string& value, const char* openAction)
{
    return Row{ .items = { Text{ .value = name, .detail = value }, Button{ .glyph = Glyph::ChevronRight, .actionId = openAction, .compact = true } },
                .clickAnywhere = true };
}

// A row that does one thing when clicked: its name, and a glyph for what it does.
Row actionRow(const std::string& name, Glyph glyph, const char* action, const std::string& detail = "")
{
    return Row{ .items = { Text{ .value = name, .detail = detail }, Button{ .glyph = glyph, .actionId = action, .compact = true } },
                .clickAnywhere = true };
}

// An on/off setting: its name and a switch (decision 053); a click anywhere on the row flips it.
Row toggleRow(const char* name, bool on, const char* actionId, const std::string& detail = "")
{
    return Row{ .items = { Text{ .value = name, .detail = detail }, Switch{ .on = on, .actionId = actionId } }, .clickAnywhere = true };
}

// One choice: its name, and a tick on the one picked. The whole row picks it.
// A theme's starts with a swatch of it: its island, and its accent in the middle.
Row choiceRow(const std::string& name, bool picked, std::string action, const Theme* theme = nullptr)
{
    Row r{ .items = { Text{ name }, Button{ .glyph = picked ? Glyph::Checked : Glyph::Unchecked, .actionId = std::move(action), .compact = true } },
           .clickAnywhere = true };
    if (theme) r.items.insert(r.items.begin(), Swatch{ .fill = theme->island, .mark = theme->accent });
    return r;
}

// "Check for updates" (decision 036): the whole row checks; under it, what
// the last check found, and until then the running version.
Row updateRow(const UpdateNote& note, const std::string& running)
{
    std::string said = note.checking ? "Checking…" : !note.found ? running : *note.found == CheckOutcome::UpToDate ? "Up to date"
                     : *note.found == CheckOutcome::Available ? note.version + " available"
                     : *note.found == CheckOutcome::Failed    ? "Couldn't check"
                                                              : "Not in dev builds";
    return Row{ .items = { Text{ .value = "Check for updates", .detail = said },
                           Button{ .glyph = Glyph::Download, .actionId = P::kCheckUpdates, .enabled = !note.checking, .compact = true } },
                .clickAnywhere = true };
}

// "Update and restart" (decision 037), while an update is offered.
Row installRow(const UpdateStatus& offer)
{
    bool installing = offer.stage == UpdateStage::Installing;
    std::string said = installing ? "Installing…" : offer.stage == UpdateStage::Failed ? offer.problem : offer.version;
    return Row{ .items = { Text{ .value = offer.stage == UpdateStage::Failed ? "Try the update again" : "Update and restart", .detail = said },
                           Button{ .glyph = Glyph::Download, .actionId = P::kInstallUpdate, .enabled = !installing, .compact = true } },
                .clickAnywhere = true };
}

// Marks an announced setting's row (decision 048): its id, a "New" tag after
// its name while unseen, and a dot on its chevron while something on its
// page is. An empty Text after the tag keeps it beside the name.
Row marked(Row r, const std::string& id, const SettingsView& v)
{
    bool announced = std::any_of(v.announced.begin(), v.announced.end(), [&](const SettingItem& s) { return s.id == id; });
    bool childUnseen = std::any_of(v.announced.begin(), v.announced.end(), [&](const SettingItem& s) { return s.parent == id && v.unseen.count(s.id); });
    r.id = id; // every setting's row is named: what's seen is counted by it
    if (!announced && !childUnseen) return r;
    if (v.unseen.count(id)) {
        auto name = std::find_if(r.items.begin(), r.items.end(), [](const Item& item) { return std::holds_alternative<Text>(item); });
        if (name != r.items.end()) {
            bool aloneText = std::count_if(r.items.begin(), r.items.end(), [](const Item& item) { return std::holds_alternative<Text>(item); }) == 1;
            auto after = r.items.insert(name + 1, Tag{ "New" });
            if (aloneText) r.items.insert(after + 1, Text{ "" });
        }
    }
    if (childUnseen)
        for (Item& item : r.items)
            if (auto* b = std::get_if<Button>(&item); b && b->glyph == Glyph::ChevronRight) b->badge = true;
    return r;
}

const char* fadeName(FadeSpeed speed) { return speed == FadeSpeed::Fast ? "Fast" : speed == FadeSpeed::Slow ? "Slow" : "Medium"; }

// --- The sidebar's pages ------------------------------------------------------

WindowPage appearancePage(const SettingsView& v)
{
    WindowPage page{ .id = kAppearancePage, .title = "Appearance", .subtitle = "How the island looks and where it sits.", .glyph = Glyph::Palette };
    std::vector<Row> look;
    if (!tokens().themes.empty()) look.push_back(marked(settingRow("Theme", v.theme, P::kOpenThemes), "theme", v));
    if (v.coverColours) look.push_back(marked(settingRow("Music colours", *v.coverColours ? "On" : "Off", P::kOpenMusicColours), "musiccolours", v));
    if (!v.displays.empty()) look.push_back(marked(settingRow("Display", v.displays[v.display], P::kOpenDisplays), "display", v));
    if (!look.empty()) page.sections.push_back({ .rows = std::move(look) });
    // Where the tab row goes (decision 048): two choices side by side, picked at once.
    Row tabs{ .items = { Text{ .value = "Tab row", .detail = "Home and pin buttons" },
                         Segmented{ .options = { "Top", "Bottom" }, .picked = v.tabsOnTop ? 0u : 1u, .actionId = P::kSetTabRow } } };
    page.sections.push_back({ .rows = { marked(tabs, "tabrow", v), marked(actionRow("Centre island", Glyph::Centre, P::kCentreIsland), "centre", v) } });
    return page;
}

WindowPage generalPage(const SettingsView& v)
{
    WindowPage page{ .id = kGeneralPage, .title = "General", .subtitle = "Starting with Windows, shortcuts and logs.", .glyph = Glyph::Sliders };
    std::vector<Row> rows;
    if (v.startsWithWindows) rows.push_back(marked(toggleRow("Start with Windows", *v.startsWithWindows, P::kToggleAutostart), "autostart", v));
    rows.push_back(marked(settingRow("Keyboard shortcuts", "Anywhere, and on the island", P::kOpenShortcuts), "shortcuts", v));
    const char* logs = v.note == LogsNote::Copied ? "Logs copied" : v.note == LogsNote::Failed ? "Couldn't copy logs" : "Copy logs";
    rows.push_back(marked(actionRow(logs, Glyph::Copy, P::kCopyLogs, "For a bug report"), "logs", v));
    page.sections.push_back({ .rows = std::move(rows) });
    return page;
}

WindowPage updatesPage(const SettingsView& v)
{
    WindowPage page{ .id = kUpdatesPage, .title = "Updates", .subtitle = "Which version this is, and newer ones.", .glyph = Glyph::Download };
    std::vector<Row> rows;
    if (v.update) rows.push_back(marked(updateRow(*v.update, v.version), "updates", v));
    if (v.update && v.offer) rows.push_back(installRow(*v.offer));
    // Which build this is: what a bug report needs (decision 036). With a
    // Check for updates row, that row says it.
    if (!v.update && !v.version.empty()) rows.push_back(row({ Text{ .value = "Version", .detail = "Cup Island " + v.version } }));
    if (!rows.empty()) page.sections.push_back({ .rows = std::move(rows) });
    return page;
}

// --- Pages within a page: one setting's choices, with a back arrow --------------

WindowPage within(WindowPage parent, const char* title, std::string subtitle)
{
    parent.title = title;
    parent.subtitle = std::move(subtitle);
    parent.backAction = P::kBack;
    parent.sections.clear();
    return parent;
}

void themesPage(WindowPage& page, const SettingsView& v)
{
    const auto& themes = tokens().themes;
    for (const ThemeListRow& r : themeListRows()) {
        if (!r.heading.empty()) page.sections.push_back({ .title = r.heading });
        else page.sections.back().rows.push_back(choiceRow(themes[r.theme].name, themes[r.theme].name == v.theme,
                                                           P::kPickTheme + std::to_string(r.theme), &themes[r.theme]));
    }
}

// The island in the music's colours (decisions 052, 053): on or off, and how fast colours fade.
void musicColoursPage(WindowPage& page, const SettingsView& v)
{
    if (v.coverColours)
        page.sections.push_back({ .rows = { marked(toggleRow("Colours from music", *v.coverColours, P::kToggleCoverColours,
                                                             "The island in the playing cover's colours"), "covercolours", v) } });
    if (!v.fadeSpeed) return;
    WindowSection fade{ .title = "Colour fade" };
    int i = 0;
    for (FadeSpeed speed : { FadeSpeed::Fast, FadeSpeed::Medium, FadeSpeed::Slow })
        fade.rows.push_back(choiceRow(fadeName(speed), speed == *v.fadeSpeed, P::kPickFade + std::to_string(i++)));
    if (v.unseen.count("fade")) fade.title += " · New";
    page.sections.push_back(std::move(fade));
}

void displaysPage(WindowPage& page, const SettingsView& v)
{
    WindowSection choices;
    for (std::size_t i = 0; i < v.displays.size(); ++i) choices.rows.push_back(choiceRow(v.displays[i], i == v.display, P::kPickDisplay + std::to_string(i)));
    page.sections.push_back(std::move(choices));
}

// The shortcuts (decision 032): the system-wide ones, each a row that takes
// new keys when clicked, then the fixed keys on the island.
void shortcutsPage(WindowPage& page, const SettingsView& v)
{
    WindowSection anywhere{ .title = "Anywhere" };
    const auto& all = commands();
    for (std::size_t i = 0; i < all.size(); ++i) {
        bool waiting = v.recording == all[i].command;
        std::string keys = waiting ? "Press keys…" : chordText(chordOf(v.shortcuts, all[i].command));
        anywhere.rows.push_back(Row{ .items = { Text{ .value = all[i].does, .detail = keys },
                                                Button{ .glyph = Glyph::Edit, .actionId = P::kEditShortcut + std::to_string(i), .compact = true } },
                                     .clickAnywhere = true });
    }
    page.sections.push_back(std::move(anywhere));
    WindowSection island{ .title = "On the island" };
    for (const FixedKey& k : fixedKeys()) island.rows.push_back(row({ Text{ .value = k.does, .detail = k.keys } }));
    page.sections.push_back(std::move(island));
}

} // namespace

std::vector<SettingItem> announcedSettings()
{
    return {
        { "tabrow", "" }, // decision 048
        { "covercolours", "musiccolours" }, // decision 052; on Music colours' page since 053
        { "musiccolours", "" },            // decision 053
        { "fade", "musiccolours" },        // decision 053
    };
}

std::string pageOfSetting(const std::string& id)
{
    if (id == "autostart" || id == "shortcuts" || id == "logs") return kGeneralPage;
    if (id == "updates") return kUpdatesPage;
    return kAppearancePage;
}

std::vector<std::string> shownSettings(const WindowContent& window, const std::string& page, const std::vector<SettingItem>& announced)
{
    std::vector<std::string> shown;
    for (const WindowPage& p : window.pages) {
        if (p.id != page) continue;
        for (const WindowSection& section : p.sections) {
            for (const Row& r : section.rows)
                if (std::any_of(announced.begin(), announced.end(), [&](const SettingItem& s) { return s.id == r.id; })) shown.push_back(r.id);
            if (section.title.rfind("Colour fade", 0) == 0) shown.push_back("fade"); // a heading, not a row
        }
    }
    return shown;
}

std::vector<ThemeListRow> themeListRows()
{
    const auto& themes = tokens().themes;
    std::vector<ThemeListRow> rows;
    for (bool light : { false, true }) {
        bool headed = false;
        for (std::size_t i = 0; i < themes.size(); ++i) {
            if (themes[i].light != light) continue;
            if (!headed) rows.push_back({ .heading = light ? "Light" : "Dark" });
            headed = true;
            rows.push_back({ .theme = i });
        }
    }
    return rows;
}

// The widget: a gear, like Windows' own Settings. A tap opens the window;
// it has no card of its own (decision 090).
Panel settingsPanel(const SettingsView& view)
{
    Content content{
        .label = "Settings",
        .badge = !view.unseen.empty(), // something new in it: a dot on its widget (decision 048)
        // One line, the gear in the side square like the other widgets' icons, bold like their titles (decision 073).
        .tileLeading = { row({ Icon{ Glyph::Settings } }, Align::Center) },
        .tile = { row({ Text{ "Settings", TextStyle::Title } }) },
    };
    return { .id = kSettingsPanelId, .content = content, .tapAction = P::kOpenWindow };
}

WindowContent settingsWindow(const SettingsView& v)
{
    WindowContent window{ .title = "Settings" };
    window.pages = { appearancePage(v), generalPage(v), updatesPage(v) };
    // A page within a page takes its parent's place in the sidebar.
    auto& appearance = window.pages[0];
    auto& general = window.pages[1];
    switch (v.page) {
    case SettingsPage::Themes: appearance = within(appearance, "Theme", "The island's colours. Dark ones, then light ones."), themesPage(appearance, v); break;
    case SettingsPage::MusicColours: appearance = within(appearance, "Music colours", ""), musicColoursPage(appearance, v); break;
    case SettingsPage::Displays: appearance = within(appearance, "Display", "The monitor the island is on."), displaysPage(appearance, v); break;
    case SettingsPage::Shortcuts: general = within(general, "Keyboard shortcuts", v.shortcutNote), shortcutsPage(general, v); break;
    case SettingsPage::List: break;
    }
    // A dot beside a page that holds something new (decision 048).
    for (WindowPage& page : window.pages)
        for (const SettingItem& s : v.announced)
            if (v.unseen.count(s.id) && pageOfSetting(s.parent.empty() ? s.id : s.parent) == page.id) page.badge = true;
    return window;
}

} // namespace cup
