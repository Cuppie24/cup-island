#pragma once
// A List card like the one Settings had before its window (decision 104):
// grouped rows of settings (decision 090), a page of themes in a long list,
// a page of keys. Keyboard navigation over lists is tested on it.

#include "core/groups.h"
#include "core/island.h"
#include "core/shortcuts.h"
#include "core/tokens.h"

#include <string>

namespace cup::test::fixture {

inline constexpr const char* kBack = "back";
inline constexpr const char* kOpenThemes = "theme.open";
inline constexpr const char* kPickTabRow = "tabrow.pick:";
inline constexpr const char* kCentre = "island.centre";
inline constexpr const char* kAutostart = "autostart.toggle";
inline constexpr const char* kOpenShortcuts = "shortcuts.open";
inline constexpr const char* kCopyLogs = "logs.copy";
inline constexpr const char* kPickTheme = "theme.pick:";
inline constexpr const char* kEditShortcut = "shortcut.edit:";
inline constexpr const char* kSettingsList = "settings";
inline constexpr const char* kThemeList = "themes";
inline constexpr const char* kShortcutList = "shortcuts";

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

inline Row button(const char* name, Glyph glyph, const char* action, std::string value = "")
{
    return groupItem(itemText(name, std::move(value)), { Button{ .glyph = glyph, .actionId = action, .compact = true } });
}

// The settings: Appearance (Theme, Tab row, Centre island), General (Start with Windows, Keyboard shortcuts, Copy logs).
inline Panel settingsList()
{
    Slot rows;
    addGroup(rows, "Appearance",
             { button("Theme", Glyph::ChevronRight, kOpenThemes, "Grape"),
               groupItem(itemText("Tab row", "Home and pin buttons"), { Segments{ .options = { "Top", "Bottom" }, .actionId = kPickTabRow } }),
               button("Centre island", Glyph::Centre, kCentre) },
             kSettingsList);
    addGroup(rows, "General",
             { groupItem(itemText("Start with Windows"), { Switch{ .on = true, .actionId = kAutostart } }),
               button("Keyboard shortcuts", Glyph::ChevronRight, kOpenShortcuts), button("Copy logs", Glyph::Copy, kCopyLogs) },
             kSettingsList);
    for (Row& r : rows) r.visibleRows = tokens().listCardVisibleRows;
    return card("fixture.settings", rows, false);
}

// The themes: a back arrow, then a list of the dark ones and the light ones under their labels.
inline Panel themesPage()
{
    Slot rows{ pageTitle("Theme") };
    const auto& themes = tokens().themes;
    for (bool light : { false, true }) {
        std::vector<Row> group;
        for (std::size_t i = 0; i < themes.size(); ++i)
            if (themes[i].light == light) group.push_back(button(themes[i].name.c_str(), Glyph::Unchecked, (kPickTheme + std::to_string(i)).c_str()));
        addGroup(rows, light ? "Light" : "Dark", std::move(group), kThemeList);
    }
    if (rows.size() > 1) rows[1].section = false; // under the title, the first label needs no gap
    return card("fixture.themes", rows, true);
}

// The keys: a back arrow, the editable ones, then the island's own, in a list longer than its window.
inline Panel shortcutsPage()
{
    Slot rows{ pageTitle("Keyboard shortcuts") };
    std::vector<Row> anywhere, island;
    const auto& all = commands();
    for (std::size_t i = 0; i < all.size(); ++i)
        anywhere.push_back(groupItem(itemText(all[i].does), { Keys{ chordText(chordOf(defaultBindings(), all[i].command)) },
                                                             Button{ .glyph = Glyph::Edit, .actionId = kEditShortcut + std::to_string(i), .compact = true } }));
    for (const FixedKey& k : fixedKeys()) island.push_back(groupItem(itemText(k.does), { Keys{ k.keys } }));
    addGroup(rows, "Anywhere", std::move(anywhere), kShortcutList);
    addGroup(rows, "On the island", std::move(island), kShortcutList);
    if (rows.size() > 1) rows[1].section = false;
    return card("fixture.keys", rows, true);
}

} // namespace cup::test::fixture
