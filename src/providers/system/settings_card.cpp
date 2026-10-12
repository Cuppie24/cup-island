#include "providers/system/settings_provider.h"

#include "core/tokens.h"

namespace cup {
namespace {

using P = SettingsProvider;

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
Row choiceRow(const std::string& name, bool picked, std::string action, const char* list)
{
    return Row{ .items = { Text{ name }, Button{ .glyph = picked ? Glyph::Checked : Glyph::Unchecked,
                                                 .actionId = std::move(action), .compact = true } },
                .clickAnywhere = true,
                .list = list,
                .listItem = true };
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
    const char* logs = v.note == LogsNote::Copied ? "Logs copied" : v.note == LogsNote::Failed ? "Couldn't copy logs" : "Copy logs";
    card.push_back(row({ Button{ .glyph = Glyph::Copy, .actionId = P::kCopyLogs, .label = logs },
                         Button{ .actionId = P::kCentreIsland, .label = "Centre island" } },
                       Align::Center));
    return card;
}

Slot themesPage(const SettingsView& v)
{
    Slot card{ pageTitle("Theme") };
    const auto& themes = tokens().themes;
    for (const ThemeListRow& r : themeListRows()) {
        if (!r.heading.empty()) card.push_back(Row{ .items = { Text{ .value = r.heading, .style = TextStyle::Caption } }, .list = P::kThemeList });
        else card.push_back(choiceRow(themes[r.theme].name, themes[r.theme].name == v.theme,
                                      P::kPickTheme + std::to_string(r.theme), P::kThemeList));
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
Panel settingsPanel(const SettingsView& view)
{
    Content content{
        .label = "Settings",
        .card = CardLayout::Centred,
        .keepOpen = true,
        .expandedCenter = view.page == SettingsPage::Themes     ? themesPage(view)
                          : view.page == SettingsPage::Displays ? displaysPage(view)
                                                                : listPage(view),
        .tile = { row({ Icon{ Glyph::Settings } }), row({ Text{ "Settings" } }) },
    };
    return { .id = SettingsProvider::kPanelId, .content = content };
}

} // namespace cup
