#pragma once
// A List card like the one Settings had before its window (decision 090):
// rows of settings in groups under headings, a page of themes in a long
// list, a page of keys. Keyboard navigation over lists is tested on it.

#include "core/island.h"
#include "core/shortcuts.h"
#include "core/tokens.h"

#include <string>

namespace cup::test::fixture {

inline constexpr const char* kBack = "back";
inline constexpr const char* kOpenThemes = "theme.open";
inline constexpr const char* kOpenTabRow = "tabrow.open";
inline constexpr const char* kCentre = "island.centre";
inline constexpr const char* kAutostart = "autostart.toggle";
inline constexpr const char* kOpenShortcuts = "shortcuts.open";
inline constexpr const char* kCopyLogs = "logs.copy";
inline constexpr const char* kPickTheme = "theme.pick:";
inline constexpr const char* kEditShortcut = "shortcut.edit:";
inline constexpr const char* kSettingsList = "settings";
inline constexpr const char* kThemeList = "themes";
inline constexpr const char* kShortcutList = "shortcuts";

inline Row listRow(std::vector<Item> items, const char* list)
{
    return Row{ .items = std::move(items), .clickAnywhere = true, .list = list, .listItem = true };
}

inline Row heading(const std::string& text, bool afterAnother, const char* list)
{
    return Row{ .items = { Text{ .value = text, .style = TextStyle::Heading } }, .list = list, .section = afterAnother, .inset = true };
}

inline Row pageTitle(const char* title)
{
    return Row{ .items = { Text{ .value = title, .style = TextStyle::Title, .centred = true } },
                .align = Align::Center, .leading = { Button{ Glyph::ChevronLeft, kBack } }, .hug = true };
}

inline Panel card(const char* id, Slot rows, bool page)
{
    Content content{ .label = id, .card = CardLayout::List, .keepOpen = true, .backAction = page ? kBack : "", .expandedCenter = std::move(rows),
                     .tile = { row({ Text{ id } }) } };
    return { .id = id, .content = content, .providerId = "fixture" };
}

// The settings: Appearance (Theme, Tab row, Centre island), General (Start with Windows, Keyboard shortcuts, Copy logs).
inline Panel settingsList()
{
    auto setting = [](const char* name, const char* value, const char* action) {
        return listRow({ Text{ name }, Text{ .value = value, .style = TextStyle::Caption, .alignEnd = true },
                         Button{ .glyph = Glyph::ChevronRight, .actionId = action, .compact = true } },
                       kSettingsList);
    };
    Slot rows{ heading("Appearance", false, kSettingsList), setting("Theme", "Grape", kOpenThemes), setting("Tab row", "Top", kOpenTabRow),
               listRow({ Text{ "Centre island" }, Button{ .glyph = Glyph::Centre, .actionId = kCentre, .compact = true } }, kSettingsList),
               heading("General", true, kSettingsList),
               listRow({ Text{ "Start with Windows" }, Switch{ .on = true, .actionId = kAutostart } }, kSettingsList),
               setting("Keyboard shortcuts", "", kOpenShortcuts),
               listRow({ Text{ "Copy logs" }, Button{ .glyph = Glyph::Copy, .actionId = kCopyLogs, .compact = true } }, kSettingsList) };
    for (Row& r : rows) r.visibleRows = tokens().listCardVisibleRows;
    return card("fixture.settings", rows, false);
}

// The themes: a back arrow, then a list of the dark ones and the light ones under their headings.
inline Panel themesPage()
{
    Slot rows{ pageTitle("Theme") };
    const auto& themes = tokens().themes;
    for (bool light : { false, true }) {
        bool headed = false;
        for (std::size_t i = 0; i < themes.size(); ++i) {
            if (themes[i].light != light) continue;
            if (!headed) rows.push_back(heading(light ? "Light" : "Dark", rows.size() > 1, kThemeList));
            headed = true;
            rows.push_back(listRow({ Text{ themes[i].name }, Button{ .glyph = Glyph::Unchecked, .actionId = kPickTheme + std::to_string(i), .compact = true } },
                                   kThemeList));
        }
    }
    return card("fixture.themes", rows, true);
}

// The keys: a back arrow, the editable ones, then the island's own, in a list longer than its window.
inline Panel shortcutsPage()
{
    Slot rows{ pageTitle("Keyboard shortcuts"), heading("Anywhere", false, kShortcutList) };
    const auto& all = commands();
    for (std::size_t i = 0; i < all.size(); ++i)
        rows.push_back(listRow({ Text{ all[i].does }, Text{ .value = chordText(chordOf(defaultBindings(), all[i].command)), .style = TextStyle::Caption, .alignEnd = true },
                                 Button{ .glyph = Glyph::Edit, .actionId = kEditShortcut + std::to_string(i), .compact = true } },
                               kShortcutList));
    rows.push_back(heading("On the island", true, kShortcutList));
    for (const FixedKey& k : fixedKeys())
        rows.push_back(Row{ .items = { Text{ k.does }, Text{ .value = k.keys, .style = TextStyle::Caption, .alignEnd = true } }, .list = kShortcutList, .inset = true });
    return card("fixture.keys", rows, true);
}

} // namespace cup::test::fixture
