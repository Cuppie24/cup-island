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

Slot listPage(const SettingsView& v)
{
    Slot card;
    if (!tokens().themes.empty()) card.push_back(settingRow("Theme", v.theme, P::kOpenThemes));
    if (!v.displays.empty()) card.push_back(settingRow("Display", v.displays[v.display], P::kOpenDisplays));
    if (v.startsWithWindows)
        card.push_back(Row{ .items = { Text{ "Start with Windows" },
                                       Button{ .glyph = *v.startsWithWindows ? Glyph::Checked : Glyph::Unchecked,
                                               .actionId = P::kToggleAutostart, .compact = true } },
                            .clickAnywhere = true,
                            .listItem = true });
    card.push_back(settingRow("Keyboard shortcuts", "", P::kOpenShortcuts));
    const char* logs = v.note == LogsNote::Copied ? "Logs copied" : v.note == LogsNote::Failed ? "Couldn't copy logs" : "Copy logs";
    card.push_back(row({ Button{ .glyph = Glyph::Copy, .actionId = P::kCopyLogs, .label = logs },
                         Button{ .actionId = P::kCentreIsland, .label = "Centre island" } },
                       Align::Center));
    // Which build this is: what a bug report needs, and what an update replaces (decision 036).
    if (!v.version.empty()) card.push_back(row({ Text{ .value = "Cup Island " + v.version, .style = TextStyle::Caption, .centred = true } }));
    return card;
}

Slot themesPage(const SettingsView& v)
{
    Slot card{ pageTitle("Theme") };
    const auto& themes = tokens().themes;
    for (const ThemeListRow& r : themeListRows()) {
        if (!r.heading.empty()) card.push_back(Row{ .items = { Text{ .value = r.heading, .style = TextStyle::Caption } }, .list = P::kThemeList });
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
    auto heading = [&](const char* text) { card.push_back(row({ Text{ .value = text, .style = TextStyle::Caption } })); };
    heading("Anywhere");
    const auto& all = commands();
    for (std::size_t i = 0; i < all.size(); ++i) {
        bool waiting = v.recording == all[i].command;
        std::string keys = waiting ? "Press keys…" : chordText(chordOf(v.shortcuts, all[i].command));
        card.push_back(Row{ .items = { Text{ all[i].does }, Text{ .value = keys, .style = TextStyle::Caption, .alignEnd = true },
                                       Button{ .glyph = Glyph::Edit, .actionId = P::kEditShortcut + std::to_string(i), .compact = true } },
                            .clickAnywhere = true,
                            .listItem = true });
    }
    heading("On the island");
    for (const FixedKey& k : fixedKeys())
        card.push_back(row({ Text{ k.does }, Text{ .value = k.keys, .style = TextStyle::Caption, .alignEnd = true } }));
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
