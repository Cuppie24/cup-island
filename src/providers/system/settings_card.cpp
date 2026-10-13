#include "providers/system/settings_card.h"

#include "core/shortcuts.h"
#include "core/tokens.h"

namespace cup {
namespace {

namespace P = settings_action;

// A setting on the list page (decision 030): its name, what it's set to, and
// a chevron that says a click opens its choices. The whole row is the button.
Row settingRow(const char* name, const std::string& value, const char* openAction)
{
    return Row{ .items = { Text{ name }, Text{ .value = value, .style = TextStyle::Caption, .alignEnd = true },
                           Button{ .glyph = Glyph::ChevronRight, .actionId = openAction, .compact = true } },
                .clickAnywhere = true,
                .listItem = true };
}

// A choice page's title, with a back arrow at its start.
Row pageTitle(const char* title)
{
    return Row{ .items = { Text{ .value = title, .style = TextStyle::Title, .centred = true } },
                .leading = { Button{ Glyph::ChevronLeft, P::kBack } } };
}

// A group's name over its rows (decision 037): heavy and full-strength, with
// room above it unless it's the first, so groups stand apart. Inset like the
// list items' text, so it lines up with their names.
Row heading(const std::string& text, bool afterAnother, const char* list = "")
{
    return Row{ .items = { Text{ .value = text, .style = TextStyle::Heading } }, .list = list, .section = afterAnother, .inset = true };
}

// One choice: its name, and a tick on the one picked. The whole row picks it.
// A theme's starts with a swatch of it: its island, and its accent in the middle.
Row choiceRow(const std::string& name, bool picked, std::string action, const char* list, const Theme* theme = nullptr)
{
    Row r{ .items = { Text{ name }, Button{ .glyph = picked ? Glyph::Checked : Glyph::Unchecked,
                                            .actionId = std::move(action), .compact = true } },
           .clickAnywhere = true,
           .list = list,
           .listItem = true };
    if (theme) r.items.insert(r.items.begin(), Swatch{ .fill = theme->island, .mark = theme->accent });
    return r;
}

// "Check for updates" (decision 036): the whole row checks; its value says
// what the last check found, and until then the running version (decision
// 037: one row less than a version line of its own). An update found is
// offered on Home, as always, and right under this row (installRow).
Row updateRow(const UpdateNote& note, const std::string& running)
{
    std::string said = note.checking ? "Checking…" : !note.found ? running : *note.found == CheckOutcome::UpToDate ? "Up to date"
                     : *note.found == CheckOutcome::Available ? note.version + " available"
                     : *note.found == CheckOutcome::Failed    ? "Couldn't check"
                                                              : "Not in dev builds";
    return Row{ .items = { Text{ "Check for updates" }, Text{ .value = said, .style = TextStyle::Caption, .alignEnd = true },
                           Button{ .glyph = Glyph::Download, .actionId = P::kCheckUpdates, .enabled = !note.checking, .compact = true } },
                .clickAnywhere = true,
                .listItem = true };
}

// "Update and restart" (decision 037), while an update is offered: the same
// install as Home's Update card. Its value says how it's going.
Row installRow(const UpdateStatus& offer)
{
    bool installing = offer.stage == UpdateStage::Installing;
    std::string said = installing ? "Installing…" : offer.stage == UpdateStage::Failed ? offer.problem : offer.version;
    return Row{ .items = { Text{ offer.stage == UpdateStage::Failed ? "Try the update again" : "Update and restart" },
                           Text{ .value = said, .style = TextStyle::Caption, .alignEnd = true },
                           Button{ .glyph = Glyph::Download, .actionId = P::kInstallUpdate, .enabled = !installing, .compact = true } },
                .clickAnywhere = true,
                .listItem = true };
}

// A row that does one thing when clicked: its name, and a glyph for what it does.
Row actionRow(const std::string& name, Glyph glyph, const char* action)
{
    return Row{ .items = { Text{ name }, Button{ .glyph = glyph, .actionId = action, .compact = true } },
                .clickAnywhere = true,
                .listItem = true };
}

// The list of settings, in groups under headings (decision 037): how the
// island looks and where it is, how it behaves, and updates. A group with
// nothing to show (no way to check for updates) has no heading.
Slot listPage(const SettingsView& v)
{
    Slot card;
    auto group = [&](const char* name, std::vector<Row> rows) {
        if (rows.empty()) return;
        card.push_back(heading(name, !card.empty()));
        for (Row& r : rows) card.push_back(std::move(r));
    };
    std::vector<Row> appearance;
    if (!tokens().themes.empty()) appearance.push_back(settingRow("Theme", v.theme, P::kOpenThemes));
    if (!v.displays.empty()) appearance.push_back(settingRow("Display", v.displays[v.display], P::kOpenDisplays));
    appearance.push_back(actionRow("Centre island", Glyph::Centre, P::kCentreIsland));
    group("Appearance", std::move(appearance));

    std::vector<Row> general;
    if (v.startsWithWindows)
        general.push_back(Row{ .items = { Text{ "Start with Windows" },
                                          Button{ .glyph = *v.startsWithWindows ? Glyph::Checked : Glyph::Unchecked,
                                                  .actionId = P::kToggleAutostart, .compact = true } },
                               .clickAnywhere = true,
                               .listItem = true });
    general.push_back(settingRow("Keyboard shortcuts", "", P::kOpenShortcuts));
    const char* logs = v.note == LogsNote::Copied ? "Logs copied" : v.note == LogsNote::Failed ? "Couldn't copy logs" : "Copy logs";
    general.push_back(actionRow(logs, Glyph::Copy, P::kCopyLogs));
    group("General", std::move(general));

    std::vector<Row> updates;
    if (v.update) updates.push_back(updateRow(*v.update, v.version));
    if (v.update && v.offer) updates.push_back(installRow(*v.offer));
    group("Updates", std::move(updates));
    // One list, sized to the card: with every row there (two monitors, an
    // update offered) it's more than fits, and scrolls; usually it all shows.
    for (Row& r : card) {
        r.list = P::kSettingsList;
        r.visibleRows = tokens().listCardVisibleRows;
    }
    // Which build this is: what a bug report needs, and what an update replaces
    // (decision 036). With a Check for updates row, that row says it.
    if (!v.version.empty() && !v.update) card.push_back(row({ Text{ .value = "Cup Island " + v.version, .style = TextStyle::Caption, .centred = true } }));
    return card;
}

Slot themesPage(const SettingsView& v)
{
    Slot card{ pageTitle("Theme") };
    const auto& themes = tokens().themes;
    for (const ThemeListRow& r : themeListRows()) {
        if (!r.heading.empty()) card.push_back(heading(r.heading, card.size() > 1, P::kThemeList));
        else card.push_back(choiceRow(themes[r.theme].name, themes[r.theme].name == v.theme,
                                      P::kPickTheme + std::to_string(r.theme), P::kThemeList, &themes[r.theme]));
    }
    return card;
}

Slot displaysPage(const SettingsView& v)
{
    Slot card{ pageTitle("Display") };
    for (std::size_t i = 0; i < v.displays.size(); ++i)
        card.push_back(choiceRow(v.displays[i], i == v.display, P::kPickDisplay + std::to_string(i), P::kDisplayList));
    return card;
}

// The shortcuts (decision 032): the system-wide ones, each a row that takes
// new keys when clicked, then the fixed keys on the island, in one list that
// scrolls (there are more than fit), then a note under it, always in view
// (waiting for keys, or why a change was refused).
Slot shortcutsPage(const SettingsView& v)
{
    Slot card{ pageTitle("Keyboard shortcuts") };
    card.push_back(heading("Anywhere", false));
    const auto& all = commands();
    for (std::size_t i = 0; i < all.size(); ++i) {
        bool waiting = v.recording == all[i].command;
        std::string keys = waiting ? "Press keys…" : chordText(chordOf(v.shortcuts, all[i].command));
        card.push_back(Row{ .items = { Text{ all[i].does }, Text{ .value = keys, .style = TextStyle::Caption, .alignEnd = true },
                                       Button{ .glyph = Glyph::Edit, .actionId = P::kEditShortcut + std::to_string(i), .compact = true } },
                            .clickAnywhere = true,
                            .listItem = true });
    }
    card.push_back(heading("On the island", true));
    for (const FixedKey& k : fixedKeys())
        card.push_back(Row{ .items = { Text{ k.does }, Text{ .value = k.keys, .style = TextStyle::Caption, .alignEnd = true } },
                            .inset = true }); // in line with the editable rows' names: nothing to click, nothing lit
    for (std::size_t i = 1; i < card.size(); ++i) card[i].list = P::kShortcutList; // all but the title
    if (!v.shortcutNote.empty()) card.push_back(row({ Text{ .value = v.shortcutNote, .style = TextStyle::Caption, .centred = true } }));
    return card;
}

} // namespace

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

// The widget: a gear, like Windows' own Settings. The card: a list of
// settings, or one setting's choices. It stays open on a click between rows
// (Content::keepOpen): a near miss on a page of controls shouldn't close it.
// Esc on one setting's page goes back to the list, like its back arrow.
Panel settingsPanel(const SettingsView& view)
{
    Content content{
        .label = "Settings",
        .card = CardLayout::List, // wider sides than Centred: rows edge to edge
        .keepOpen = true,
        .backAction = view.page == SettingsPage::List ? "" : P::kBack, // Esc on a page: back to the list (decision 033)
        .expandedCenter = view.page == SettingsPage::Themes      ? themesPage(view)
                          : view.page == SettingsPage::Displays  ? displaysPage(view)
                          : view.page == SettingsPage::Shortcuts ? shortcutsPage(view)
                                                                 : listPage(view),
        .tile = { row({ Icon{ Glyph::Settings } }), row({ Text{ "Settings" } }) },
    };
    return { .id = kSettingsPanelId, .content = content };
}

} // namespace cup
