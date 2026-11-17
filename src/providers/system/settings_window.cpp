#include "providers/system/settings_window.h"

#include "core/groups.h"
#include "core/shortcuts.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

namespace P = settings_action;

// A setting with choices (decision 030): its name, what it's set to under it
// (decision 090), and a chevron: the whole row opens its choices.
Row settingRow(const char* name, const std::string& value, const char* openAction)
{
    return groupItem(itemText(name, value), { Button{ .glyph = Glyph::ChevronRight, .actionId = openAction, .compact = true } });
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
// updates", and under it what the last check found, until then the running
// version. An update offered (by any check): "Update and restart", the
// running and offered versions under it. The only place updates are offered (decision 152).
Row updateRow(const UpdateNote& note, const std::string& running, const std::optional<UpdateStatus>& offer)
{
    if (offer) {
        bool installing = offer->stage == UpdateStage::Installing;
        std::string versions = running.empty() ? offer->version : running + " → " + offer->version;
        std::string said = installing                            ? "Installing…"
                           : offer->stage == UpdateStage::Failed ? offer->problem
                           : offer->mandatory                    ? versions + ", required: installs at the next start" // decision 041
                                                                 : versions;
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
Row actionRow(const std::string& name, Glyph glyph, const char* action, std::string description = "")
{
    return groupItem(itemText(name, std::move(description)), { Button{ .glyph = glyph, .actionId = action, .compact = true } });
}

// An on/off setting: its name and a switch (decision 053); a click anywhere on the row flips it.
Row toggleRow(const char* name, bool on, const char* actionId, std::string description = "")
{
    return groupItem(itemText(name, std::move(description)), { Switch{ .on = on, .actionId = actionId } });
}

// A few options picked in place (decision 093).
Row segmentsRow(const char* name, const char* description, std::vector<std::string> options, std::size_t picked, const char* action)
{
    return groupItem(itemText(name, description), { Segments{ .options = std::move(options), .picked = picked, .actionId = action } });
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
const char* transparencyName(Transparency level) { return level == Transparency::Low ? "Low" : level == Transparency::High ? "High" : "Medium"; }

// What the Window theme row says it's set to (decision 150).
std::string windowThemeValue(const SettingsView& v)
{
    if (v.windowTheme == WindowThemeMode::System) return "Like Windows";
    if (v.windowTheme == WindowThemeMode::Manual) return v.windowThemeName;
    return "Like the island";
}

// --- The sidebar's pages ------------------------------------------------------

WindowPage appearancePage(const SettingsView& v)
{
    WindowPage page{ .id = kAppearancePage, .title = "Appearance", .subtitle = "How the island looks and where it sits.", .glyph = Glyph::Palette };
    std::vector<Row> look;
    if (!tokens().themes.empty()) look.push_back(marked(settingRow("Theme", v.theme, P::kOpenThemes), "theme", v));
    if (!tokens().themes.empty() && v.windowTheme) look.push_back(marked(settingRow("Window theme", windowThemeValue(v), P::kOpenWindowTheme), "windowtheme", v));
    if (v.glass) look.push_back(marked(settingRow("Glass", *v.glass ? "On" : "Off", P::kOpenGlass), "glass", v)); // decisions 089, 096
    if (v.coverColours) look.push_back(marked(settingRow("Music colours", *v.coverColours ? "On" : "Off", P::kOpenMusicColours), "musiccolours", v));
    if (!v.displays.empty()) look.push_back(marked(settingRow("Display", v.displays[v.display], P::kOpenDisplays), "display", v));
    if (!look.empty()) page.sections.push_back({ .rows = std::move(look) });
    // Picked in place (decision 093): two options don't need a page.
    page.sections.push_back({ .rows = { marked(segmentsRow("Tab row", "Home and pin buttons", { "Top", "Bottom" }, v.tabsOnTop ? 0u : 1u, P::kPickTabRow), "tabrow", v),
                                        marked(actionRow("Centre island", Glyph::Centre, P::kCentreIsland), "centre", v) } });
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
    // With no way to check, which build this is still shows: what a bug report needs (decision 036).
    std::vector<Row> rows;
    if (v.update) rows.push_back(marked(updateRow(*v.update, v.version, v.offer), "updates", v));
    else if (!v.version.empty()) rows.push_back(groupItem(itemText("Version", "Cup Island " + v.version)));
    // What's new (decision 152): the offer's release notes, or this version's.
    if (v.opensPages && !whatsNewPage(v).empty()) {
        std::string in = v.offer && !v.offer->page.empty() ? v.offer->version : v.version;
        rows.push_back(actionRow("What's new", Glyph::OpenApp, P::kWhatsNew, "In " + in + ", on GitHub"));
    }
    // An install that failed: the same installer, to download and run by hand (it can't get stuck the same way).
    if (v.opensPages && v.offer && v.offer->stage == UpdateStage::Failed && !v.offer->download.empty())
        rows.push_back(actionRow("Download installer", Glyph::Download, P::kDownloadUpdate, "Cup Island " + v.offer->version + ", to run yourself"));
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

// Every theme, dark ones then light ones under their headings, `picked` ticked; a click sends `action` + its index.
void themeList(WindowPage& page, const std::string& picked, const char* action)
{
    const auto& themes = tokens().themes;
    for (const ThemeListRow& r : themeListRows()) {
        if (!r.heading.empty()) page.sections.push_back({ .title = r.heading });
        else page.sections.back().rows.push_back(choiceRow(themes[r.theme].name, themes[r.theme].name == picked, action + std::to_string(r.theme), &themes[r.theme]));
    }
}

void themesPage(WindowPage& page, const SettingsView& v) { themeList(page, v.theme, P::kPickTheme); }

// Windows' own theme (decision 150): like the island, like Windows, or picked
// in place (decision 093); the themes to pick from only while it's Manual.
void windowThemePage(WindowPage& page, const SettingsView& v)
{
    WindowThemeMode mode = v.windowTheme.value_or(WindowThemeMode::Island);
    const char* says = mode == WindowThemeMode::System ? "Light or dark, as Windows' apps are"
                     : mode == WindowThemeMode::Manual ? "The theme picked below"
                                                       : "The island's theme and music colours";
    page.sections.push_back({ .rows = { segmentsRow("Colours", says, { "Island", "System", "Manual" }, static_cast<std::size_t>(mode), P::kPickWindowThemeMode) } });
    if (mode == WindowThemeMode::Manual) themeList(page, v.windowThemeName, P::kPickWindowTheme);
}

// The island in the music's colours (decisions 052, 053): on or off, and how
// fast colours fade from one cover (or theme) to the next, picked in place.
void musicColoursPage(WindowPage& page, const SettingsView& v)
{
    WindowSection rows;
    if (v.coverColours) rows.rows.push_back(marked(toggleRow("Colours from music", *v.coverColours, P::kToggleCoverColours), "covercolours", v));
    if (v.fadeSpeed) {
        std::size_t picked = *v.fadeSpeed == FadeSpeed::Fast ? 0 : *v.fadeSpeed == FadeSpeed::Slow ? 2 : 1;
        rows.rows.push_back(marked(segmentsRow("Colour fade", "From one cover to the next",
                                               { fadeName(FadeSpeed::Fast), fadeName(FadeSpeed::Medium), fadeName(FadeSpeed::Slow) }, picked, P::kPickFade),
                                   "fade", v));
    }
    page.sections.push_back(std::move(rows));
}

// The island as glass (decisions 089, 096): on or off, and how see-through.
void glassPage(WindowPage& page, const SettingsView& v)
{
    WindowSection rows;
    if (v.glass) rows.rows.push_back(toggleRow("Glass effect", *v.glass, P::kToggleGlass));
    if (v.transparency) {
        std::size_t picked = *v.transparency == Transparency::Low ? 0 : *v.transparency == Transparency::High ? 2 : 1;
        rows.rows.push_back(marked(segmentsRow("Transparency", "How much shows through",
                                               { transparencyName(Transparency::Low), transparencyName(Transparency::Medium), transparencyName(Transparency::High) },
                                               picked, P::kPickTransparency),
                                   "transparency", v));
    }
    // Windows too (decision 104): Settings and providers' other windows, see-through like the island.
    if (v.glassWindows)
        rows.rows.push_back(marked(toggleRow("Glass for windows", *v.glassWindows, P::kToggleGlassWindows, "Settings and other windows, like the island"),
                                   "glasswindows", v));
    page.sections.push_back(std::move(rows));
}

void displaysPage(WindowPage& page, const SettingsView& v)
{
    WindowSection choices;
    for (std::size_t i = 0; i < v.displays.size(); ++i) choices.rows.push_back(choiceRow(v.displays[i], i == v.display, P::kPickDisplay + std::to_string(i)));
    page.sections.push_back(std::move(choices));
}

// The shortcuts (decision 032): the system-wide ones, each a row that takes
// new keys when clicked and goes back to its default (decision 103), then the
// fixed keys on the island. Keys as key caps (decision 092).
void shortcutsPage(WindowPage& page, const SettingsView& v)
{
    auto keyRow = [](const std::string& does, Item keys, std::vector<Item> end) {
        end.insert(end.begin(), std::move(keys));
        return groupItem(itemText(does), std::move(end));
    };
    WindowSection anywhere{ .title = "Anywhere" }, island{ .title = "On the island" };
    const auto& all = commands();
    bool anyChanged = false;
    for (std::size_t i = 0; i < all.size(); ++i) {
        Item keys = v.recording == all[i].command ? Item{ Text{ .value = "Press keys…", .style = TextStyle::Caption, .alignEnd = true } }
                                                  : Item{ Keys{ chordText(chordOf(v.shortcuts, all[i].command)) } };
        bool changed = chordOf(v.shortcuts, all[i].command) != all[i].standard; // dimmed while the keys are the default
        anyChanged = anyChanged || changed;
        anywhere.rows.push_back(keyRow(all[i].does, std::move(keys),
                                       { Button{ .glyph = Glyph::Reset, .actionId = P::kResetShortcut + std::to_string(i), .enabled = changed, .compact = true },
                                         Button{ .glyph = Glyph::Edit, .actionId = P::kEditShortcut + std::to_string(i), .compact = true } }));
    }
    anywhere.rows.push_back(groupItem(itemText("Reset all to defaults"),
                                      { Button{ .glyph = Glyph::Reset, .actionId = P::kResetShortcuts, .enabled = anyChanged, .compact = true } }));
    for (const FixedKey& k : fixedKeys()) island.rows.push_back(keyRow(k.does, Keys{ k.keys }, {})); // nothing to click, nothing lit
    page.sections.push_back(std::move(anywhere));
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
        { "glass", "" },                   // decision 089
        { "transparency", "glass" },       // decision 096
        { "glasswindows", "glass" },       // decision 104
        { "windowtheme", "" },             // decision 150
    };
}

std::string pageOfSetting(const std::string& id)
{
    if (id == "autostart" || id == "shortcuts" || id == "logs") return kGeneralPage;
    if (id == "updates") return kUpdatesPage;
    return kAppearancePage;
}

std::set<std::string> offeredSettings(const SettingsView& view)
{
    std::set<std::string> offered;
    for (SettingsPage page : { SettingsPage::List, SettingsPage::Themes, SettingsPage::WindowTheme, SettingsPage::Displays, SettingsPage::Shortcuts,
                               SettingsPage::MusicColours, SettingsPage::Glass }) {
        SettingsView v = view;
        v.page = page;
        WindowContent window = settingsWindow(v);
        for (const WindowPage& p : window.pages)
            for (const std::string& id : shownSettings(window, p.id, view.announced)) offered.insert(id);
    }
    return offered;
}

std::vector<std::string> shownSettings(const WindowContent& window, const std::string& page, const std::vector<SettingItem>& announced)
{
    std::vector<std::string> shown;
    for (const WindowPage& p : window.pages)
        if (p.id == page)
            for (const WindowSection& section : p.sections)
                for (const Row& r : section.rows)
                    if (std::any_of(announced.begin(), announced.end(), [&](const SettingItem& s) { return s.id == r.id; })) shown.push_back(r.id);
    return shown;
}

bool updateWaiting(const SettingsView& v) { return v.offer && v.offer->stage != UpdateStage::Installing; }

std::string whatsNewPage(const SettingsView& v)
{
    if (v.offer && !v.offer->page.empty()) return v.offer->page;
    if (auto running = parseVersion(v.version)) return releasePage(*running);
    return ""; // a dev build: no release of its own
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

// A gear, like Windows' own Settings, in the page row right of the pin
// (decision 113), not a Home widget. A tap opens the window; it has no card
// of its own (decision 104).
Panel settingsPanel(const SettingsView& view)
{
    Content content{
        .label = "Settings",
        .badge = !view.unseen.empty(), // something new in it: a dot on its button (decision 048)
        .badgeAccent = updateWaiting(view), // an update to install: the dot in the accent instead (decision 152)
    };
    return { .id = kSettingsPanelId, .content = content, .tapAction = P::kOpenWindow, .rowButton = Glyph::Settings };
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
    case SettingsPage::WindowTheme: appearance = within(appearance, "Window theme", "Settings and other windows' colours."), windowThemePage(appearance, v); break;
    case SettingsPage::MusicColours: appearance = within(appearance, "Music colours", ""), musicColoursPage(appearance, v); break;
    case SettingsPage::Glass: appearance = within(appearance, "Glass", ""), glassPage(appearance, v); break;
    case SettingsPage::Displays: appearance = within(appearance, "Display", "The monitor the island is on."), displaysPage(appearance, v); break;
    case SettingsPage::Shortcuts: general = within(general, "Keyboard shortcuts", v.shortcutNote), shortcutsPage(general, v); break;
    case SettingsPage::List: break;
    }
    // A dot beside a page that holds something new (decision 048); an update waiting, in the accent (decision 152).
    window.pages[2].badgeAccent = updateWaiting(v);
    for (WindowPage& page : window.pages)
        for (const SettingItem& s : v.announced)
            if (v.unseen.count(s.id) && pageOfSetting(s.parent.empty() ? s.id : s.parent) == page.id) page.badge = true;
    return window;
}

} // namespace cup
