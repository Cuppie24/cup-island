#include "providers/system/settings_card.h"

#include "core/groups.h"
#include "core/shortcuts.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

namespace P = settings_action;

// A setting on the list page (decision 030): its name, what it's set to
// under it (decision 090), and a chevron that says a click opens its
// choices. The whole row is the button.
Row settingRow(const char* name, const std::string& value, const char* openAction)
{
    return groupItem(itemText(name, value), { Button{ .glyph = Glyph::ChevronRight, .actionId = openAction, .compact = true } });
}

// A choice page's title, with a back arrow at its start. Hugging: the title
// as wide as it is, centred on the card, not in the room after the arrow.
Row pageTitle(const char* title)
{
    return Row{ .items = { Text{ .value = title, .style = TextStyle::Title, .centred = true } },
                .align = Align::Center, .leading = { Button{ Glyph::ChevronLeft, P::kBack } }, .hug = true };
}

// One choice: its name, and a tick on the one picked. The whole row picks it.
// A theme's starts with a swatch of it: its island, and its accent in the middle.
Row choiceRow(const std::string& name, bool picked, std::string action, const Theme* theme = nullptr)
{
    Row r = groupItem(itemText(name), { Button{ .glyph = picked ? Glyph::Checked : Glyph::Unchecked, .actionId = std::move(action), .compact = true } });
    if (theme) r.items.insert(r.items.begin(), Swatch{ .fill = theme->island, .mark = theme->accent });
    return r;
}

// The one updates row (decisions 036, 091). Nothing offered: "Check for
// updates", a refresh glyph, and under it what the last check found, until
// then the running version. An update offered (by any check): the same row
// turns into "Update and restart", the running and offered versions under it
// and the download glyph: the same install as Home's Update card.
Row updateRow(const UpdateNote& note, const std::string& running, const std::optional<UpdateStatus>& offer)
{
    if (offer) {
        bool installing = offer->stage == UpdateStage::Installing;
        std::string versions = running.empty() ? offer->version : running + " → " + offer->version;
        std::string said = installing ? "Installing…" : offer->stage == UpdateStage::Failed ? offer->problem : versions;
        return groupItem(itemText(offer->stage == UpdateStage::Failed ? "Try the update again" : "Update and restart", said),
                         { Button{ .glyph = Glyph::Download, .actionId = P::kInstallUpdate, .enabled = !installing, .compact = true } });
    }
    std::string said = note.checking ? "Checking…" : !note.found ? (running.empty() ? "" : "Version " + running) : *note.found == CheckOutcome::UpToDate ? "Up to date"
                     : *note.found == CheckOutcome::Available ? note.version + " available"
                     : *note.found == CheckOutcome::Failed    ? "Couldn't check"
                                                              : "Not in dev builds";
    return groupItem(itemText("Check for updates", said),
                     { Button{ .glyph = Glyph::Refresh, .actionId = P::kCheckUpdates, .enabled = !note.checking, .compact = true } });
}

// A row that does one thing when clicked: its name, and a glyph for what it does.
Row actionRow(const std::string& name, Glyph glyph, const char* action)
{
    return groupItem(itemText(name), { Button{ .glyph = glyph, .actionId = action, .compact = true } });
}

// Marks an announced setting's row (decision 048): its id, a "New" tag after
// its name while unseen, and a dot on its chevron while something on its
// page is. An empty Text after the tag keeps it beside the name, not pushed
// to the far end (the texts' widths, decision 037).
Row marked(Row r, const std::string& id, const SettingsView& v)
{
    bool announced = std::any_of(v.announced.begin(), v.announced.end(), [&](const SettingItem& s) { return s.id == id; });
    bool childUnseen = std::any_of(v.announced.begin(), v.announced.end(), [&](const SettingItem& s) { return s.parent == id && v.unseen.count(s.id); });
    if (!announced && !childUnseen) return r;
    r.id = id;
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

// An on/off setting: its name and a switch (decision 053); a click anywhere on the row flips it.
Row toggleRow(const char* name, bool on, const char* actionId)
{
    return groupItem(itemText(name), { Switch{ .on = on, .actionId = actionId } });
}

const char* fadeName(FadeSpeed speed)
{
    return speed == FadeSpeed::Fast ? "Fast" : speed == FadeSpeed::Slow ? "Slow" : "Medium";
}

// The list of settings, in groups under labels (decisions 037, 090): how the
// island looks and where it is, how it behaves, and updates. A group with
// nothing to show (no way to check for updates) has no label.
Slot listPage(const SettingsView& v)
{
    Slot card;
    auto group = [&](const char* name, std::vector<Row> rows) { addGroup(card, name, std::move(rows)); };
    std::vector<Row> appearance;
    if (!tokens().themes.empty()) appearance.push_back(marked(settingRow("Theme", v.theme, P::kOpenThemes), "theme", v));
    if (v.glass) appearance.push_back(marked(toggleRow("Glass", *v.glass, P::kToggleGlass), "glass", v)); // decision 089
    if (v.coverColours) appearance.push_back(marked(settingRow("Music colours", *v.coverColours ? "On" : "Off", P::kOpenMusicColours), "musiccolours", v));
    if (!v.displays.empty()) appearance.push_back(marked(settingRow("Display", v.displays[v.display], P::kOpenDisplays), "display", v));
    // Picked in place (decision 093): two options don't need a page.
    appearance.push_back(marked(groupItem(itemText("Tab row", "Home and pin buttons"),
                                          { Segments{ .options = { "Top", "Bottom" }, .picked = v.tabsOnTop ? 0u : 1u, .actionId = P::kPickTabRow } }),
                                "tabrow", v));
    appearance.push_back(marked(actionRow("Centre island", Glyph::Centre, P::kCentreIsland), "centre", v));
    group("Appearance", std::move(appearance));

    std::vector<Row> general;
    if (v.startsWithWindows) general.push_back(marked(toggleRow("Start with Windows", *v.startsWithWindows, P::kToggleAutostart), "autostart", v));
    general.push_back(marked(settingRow("Keyboard shortcuts", "", P::kOpenShortcuts), "shortcuts", v));
    const char* logs = v.note == LogsNote::Copied ? "Logs copied" : v.note == LogsNote::Failed ? "Couldn't copy logs" : "Copy logs";
    general.push_back(marked(actionRow(logs, Glyph::Copy, P::kCopyLogs), "logs", v));
    group("General", std::move(general));

    std::vector<Row> updates;
    if (v.update) updates.push_back(marked(updateRow(*v.update, v.version, v.offer), "updates", v));
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
    std::string label;
    std::vector<Row> rows;
    auto flush = [&] { addGroup(card, label, std::move(rows), P::kThemeList), rows.clear(); };
    for (const ThemeListRow& r : themeListRows()) {
        if (!r.heading.empty()) flush(), label = r.heading;
        else rows.push_back(choiceRow(themes[r.theme].name, themes[r.theme].name == v.theme, P::kPickTheme + std::to_string(r.theme), &themes[r.theme]));
    }
    flush();
    if (card.size() > 1) card[1].section = false; // under the title, the first label needs no gap
    return card;
}

// The island in the music's colours (decisions 052, 053): on or off, and how
// fast colours fade from one cover (or theme) to the next, picked in place
// (decision 093). Picks apply at once.
Slot musicColoursPage(const SettingsView& v)
{
    Slot card{ pageTitle("Music colours") };
    std::vector<Row> rows;
    if (v.coverColours) rows.push_back(marked(toggleRow("Colours from music", *v.coverColours, P::kToggleCoverColours), "covercolours", v));
    if (v.fadeSpeed) {
        std::size_t picked = *v.fadeSpeed == FadeSpeed::Fast ? 0 : *v.fadeSpeed == FadeSpeed::Slow ? 2 : 1;
        rows.push_back(marked(groupItem(itemText("Colour fade", "From one cover to the next"),
                                        { Segments{ .options = { fadeName(FadeSpeed::Fast), fadeName(FadeSpeed::Medium), fadeName(FadeSpeed::Slow) },
                                                    .picked = picked, .actionId = P::kPickFade } }),
                              "fade", v));
    }
    addGroup(card, "", std::move(rows), P::kMusicColoursList);
    if (card.size() > 1) card[1].section = false; // right under the title
    return card;
}

Slot displaysPage(const SettingsView& v)
{
    Slot card{ pageTitle("Display") };
    std::vector<Row> rows;
    for (std::size_t i = 0; i < v.displays.size(); ++i) rows.push_back(choiceRow(v.displays[i], i == v.display, P::kPickDisplay + std::to_string(i)));
    addGroup(card, "", std::move(rows), P::kDisplayList);
    if (card.size() > 1) card[1].section = false;
    return card;
}

// The shortcuts (decision 032): the system-wide ones, each a row that takes
// new keys when clicked, then the fixed keys on the island, in one list that
// scrolls (there are more than fit), then a note under it, always in view
// (waiting for keys, or why a change was refused).
Slot shortcutsPage(const SettingsView& v)
{
    Slot card{ pageTitle("Keyboard shortcuts") };
    // A key's row: what it does, its keys at the end as key caps (decision 092).
    // Waiting for new keys, a note there instead.
    auto keyRow = [](const std::string& does, Item keys, std::vector<Item> end) {
        end.insert(end.begin(), std::move(keys));
        return groupItem(itemText(does), std::move(end));
    };
    std::vector<Row> anywhere, island;
    const auto& all = commands();
    for (std::size_t i = 0; i < all.size(); ++i) {
        Item keys = v.recording == all[i].command ? Item{ Text{ .value = "Press keys…", .style = TextStyle::Caption, .alignEnd = true } }
                                                  : Item{ Keys{ chordText(chordOf(v.shortcuts, all[i].command)) } };
        anywhere.push_back(keyRow(all[i].does, std::move(keys), { Button{ .glyph = Glyph::Edit, .actionId = P::kEditShortcut + std::to_string(i), .compact = true } }));
    }
    for (const FixedKey& k : fixedKeys()) island.push_back(keyRow(k.does, Keys{ k.keys }, {})); // nothing to click, nothing lit
    addGroup(card, "Anywhere", std::move(anywhere), P::kShortcutList);
    addGroup(card, "On the island", std::move(island), P::kShortcutList);
    if (card.size() > 1) card[1].section = false;
    // One list as tall as the card allows, like the root page: more keys in view at once.
    for (std::size_t i = 1; i < card.size(); ++i) card[i].visibleRows = tokens().listCardVisibleRows;
    if (!v.shortcutNote.empty()) card.push_back(row({ Text{ .value = v.shortcutNote, .style = TextStyle::Caption, .centred = true } }));
    return card;
}

} // namespace

std::vector<SettingItem> announcedSettings()
{
    return {
        { "tabrow", "" }, // decision 048
        { "covercolours", "musiccolours" }, // decision 052; on Music colours' page since 053
        { "musiccolours", "" },            // decision 053
        { "fade", "musiccolours" },        // decision 053
        { "glass", "" },                   // decision 089
    };
}

std::vector<std::string> shownSettings(const Panel& panel, const std::vector<SettingItem>& announced)
{
    std::vector<std::string> shown;
    for (const Row& r : panel.content.expandedCenter)
        if (std::any_of(announced.begin(), announced.end(), [&](const SettingItem& s) { return s.id == r.id; })) shown.push_back(r.id);
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

// The widget: a gear, like Windows' own Settings. The card: a list of
// settings, or one setting's choices. It stays open on a click between rows
// (Content::keepOpen): a near miss on a page of controls shouldn't close it.
// Esc on one setting's page goes back to the list, like its back arrow.
Panel settingsPanel(const SettingsView& view)
{
    Content content{
        .label = "Settings",
        .card = CardLayout::List, // wider sides than Centred: rows edge to edge
        .badge = !view.unseen.empty(), // something new in it: a dot on its widget and tab (decision 048)
        .keepOpen = true,
        .backAction = view.page == SettingsPage::List ? "" : P::kBack, // Esc on a page: back to the list (decision 033)
        .expandedCenter = view.page == SettingsPage::Themes      ? themesPage(view)
                          : view.page == SettingsPage::Displays  ? displaysPage(view)
                          : view.page == SettingsPage::MusicColours ? musicColoursPage(view)
                          : view.page == SettingsPage::Shortcuts ? shortcutsPage(view)
                                                                 : listPage(view),
        // One line, the gear in the side square like the other widgets' icons, bold like their titles (decision 073).
        .tileLeading = { row({ Icon{ Glyph::Settings } }, Align::Center) },
        .tile = { row({ Text{ "Settings", TextStyle::Title } }) },
    };
    return { .id = kSettingsPanelId, .content = content };
}

} // namespace cup
