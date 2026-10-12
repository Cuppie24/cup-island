// The Settings widget (decisions 022, 023, 030): its list of settings, the
// theme and display pages, Copy logs, Centre island, Start with Windows.
// Its keyboard shortcuts page: shortcut_settings_tests.cpp.

#include "expect.h"
#include "panel_host.h"
#include "core/tokens.h"
#include "providers/provider_registry.h"
#include "providers/system/settings_provider.h"

#include <memory>

namespace cup::test {

void runSettingsTests()
{
    // Settings (decisions 022, 030): a widget whose card is a list of settings.
    Tokens saved = tokens();
    Tokens themed = saved;
    Theme grape{ .name = "Grape", .colors = { { "islandColor", { 24, 9, 42 } } }, .island = { 24, 9, 42 }, .accent = { 191, 90, 242 } };
    themed.themes = { { "Midnight", {} }, { .name = "Paper", .island = { 246, 246, 248 }, .light = true }, grape, { "Lime", {} } };
    setTokens(themed);
    PanelHost sh;
    std::string& stored = sh.store[SettingsProvider::kThemeKey];
    stored = "Lime\r\n";
    std::string applied;
    bool copyWorks = true;
    int copies = 0, centred = 0;
    const int& saves = sh.saves[SettingsProvider::kThemeKey];
    SettingsProvider settings({ .applyTheme = [&](const std::string& name) { applied = name; },
                                .copyLogs = [&] { ++copies; return copyWorks; },
                                .centreIsland = [&] { ++centred; } });
    settings.start(sh);
    const Panel& widget = sh.published.back();
    expect(widget.id == SettingsProvider::kPanelId && widget.tapAction.empty() && hasCard(widget.content) &&
           widget.content.card == CardLayout::List && widget.content.keepOpen,
           "settings: a widget that opens a list card, which a click between rows doesn't close");
    expect(glyphAt(widget, 0) == Glyph::Settings && textAt(widget, 1, 0) == "Settings", "settings: the gear");
    expect(applied == "Lime" && saves == 0 && cardText(widget, 0) == "Theme" && cardItem(widget, 0, 1) == "Lime",
           "settings: the saved theme applies at start, and its row says so");
    const Row& themeRow = widget.content.expandedCenter[0];
    expect(themeRow.listItem && themeRow.clickAnywhere && std::get<Button>(themeRow.items[2]).actionId == settings_action::kOpenThemes,
           "settings: the theme row is a list item; a click anywhere on it opens the themes");

    auto press = [&](const std::string& actionId) { settings.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    press(settings_action::kOpenThemes);
    const Slot& page = sh.published.back().content.expandedCenter;
    expect(page.size() == 7 && cardText(sh.published.back(), 0) == "Theme" &&
           std::get<Button>(page[0].leading[0]).actionId == settings_action::kBack, "themes: a page with a back arrow");
    expect(cardText(sh.published.back(), 1) == "Dark" && cardItem(sh.published.back(), 2, 1) == "Midnight" &&
           cardItem(sh.published.back(), 3, 1) == "Grape" && cardItem(sh.published.back(), 4, 1) == "Lime" &&
           cardText(sh.published.back(), 5) == "Light" && cardItem(sh.published.back(), 6, 1) == "Paper",
           "themes: dark ones, then light ones, each under its heading");
    expect(std::get<Swatch>(page[3].items[0]) == Swatch{ .fill = { 24, 9, 42 }, .mark = { 191, 90, 242 } },
           "themes: each starts with a swatch of its island and accent");
    expect(std::get<Button>(page[4].items[2]).glyph == Glyph::Checked && std::get<Button>(page[2].items[2]).glyph == Glyph::Unchecked &&
           page[4].list == settings_action::kThemeList && page[1].list == settings_action::kThemeList,
           "themes: one scrolling list, the picked one ticked");
    expect(sh.revealed == std::string(settings_action::kThemeList) + "@3", "themes: scrolled to the picked one");
    press(std::string(settings_action::kPickTheme) + "1");
    const Slot& picked = sh.published.back().content.expandedCenter;
    expect(applied == "Paper" && stored == "Paper" && saves == 1 && picked.size() == 7 &&
           std::get<Button>(picked[6].items[2]).glyph == Glyph::Checked && std::get<Button>(picked[4].items[2]).glyph == Glyph::Unchecked,
           "themes: a pick applies and saves, and the page stays, the tick moved: the island is the preview");
    press(settings_action::kBack);
    expect(cardText(sh.published.back(), 0) == "Theme" && cardItem(sh.published.back(), 0, 1) == "Paper", "themes: back shows the list, with the pick");
    press(settings_action::kOpenThemes);
    press(settings_action::kBack);
    press(std::string(settings_action::kPickTheme) + "9");
    expect(applied == "Paper" && saves == 1 && cardItem(sh.published.back(), 0, 1) == "Paper",
           "themes: back changes nothing; a choice that's gone is ignored");

    stored = "Renamed";
    settings.stop();
    settings.start(sh);
    expect(applied == "Midnight", "settings: a saved theme tokens.json no longer has falls back to the default");
    themed.themes.erase(themed.themes.begin());
    setTokens(themed);
    settings.refresh();
    expect(applied == "Paper" && cardItem(sh.published.back(), 0, 1) == "Paper", "settings: a reload that drops the theme moves to the new first");


    // Copy logs moved here from its own tile: says how the copy went, then resets.
    expect(cardText(sh.published.back(), 2) == "Copy logs" && !sh.wake, "logs: idle, nothing scheduled");
    sh.clock = Ms{ 1000 };
    press(settings_action::kCopyLogs);
    expect(copies == 1 && cardText(sh.published.back(), 2) == "Logs copied", "logs: a click copies and says so");
    expect(sh.wake == Ms{ 1000 } + tokens().logsNote, "logs: the note lasts logsNoteMs");
    settings.onWake();
    expect(cardText(sh.published.back(), 2) == "Copy logs", "logs: then back to Copy logs");
    press(settings_action::kCentreIsland);
    const Row& buttons = sh.published.back().content.expandedCenter[2];
    expect(centred == 1 && buttons.items.size() == 2 && std::get<Button>(buttons.items[1]).label == "Centre island",
           "settings: Centre island beside Copy logs puts a moved island back (decision 026)");
    copyWorks = false;
    press(settings_action::kCopyLogs);
    expect(cardText(sh.published.back(), 2) == "Couldn't copy logs", "logs: a failed copy says so");
    settings.stop();
    expect(sh.removed.back() == SettingsProvider::kPanelId && !sh.wake, "settings: stop removes the widget and the wake-up");

    themed.themes.clear();
    setTokens(themed);
    settings.start(sh);
    expect(sh.published.back().content.expandedCenter.size() == 2 && cardText(sh.published.back(), 1) == "Copy logs",
           "settings: no themes in tokens.json, no theme row");
    settings.stop();

    // Start with Windows (decision 030): on the first time, then as the user leaves it.
    bool autostart = false;
    int autostartWrites = 0;
    std::string& savedAutostart = sh.store[SettingsProvider::kAutostartKey];
    SettingsProvider startup({ .startsWithWindows = [&] { return autostart; },
                               .setStartsWithWindows = [&](bool on) { autostart = on; ++autostartWrites; } });
    startup.start(sh);
    const Row& startupRow = sh.published.back().content.expandedCenter[0];
    expect(autostart && savedAutostart == "on" && cardText(sh.published.back(), 0) == "Start with Windows" &&
           std::get<Button>(startupRow.items[1]).glyph == Glyph::Checked && startupRow.clickAnywhere,
           "startup: on by default, ticked");
    auto pressStartup = [&](const char* actionId) { startup.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    pressStartup(settings_action::kToggleAutostart);
    expect(!autostart && savedAutostart == "off" &&
           std::get<Button>(sh.published.back().content.expandedCenter[0].items[1]).glyph == Glyph::Unchecked,
           "startup: a click turns it off, saved");
    startup.stop();
    startup.start(sh);
    expect(!autostart && autostartWrites == 2, "startup: off stays off after a restart");
    autostart = true; // turned on elsewhere
    pressStartup(settings_action::kToggleAutostart);
    expect(!autostart && savedAutostart == "off", "startup: the row reads the real state, not the saved one");
    startup.stop();


    // Display picker (decision 023): "Main display", then each monitor, by name.
    std::vector<DisplayChoice> monitors{ { "A", "Home LCD" } };
    std::string& savedDisplay = sh.store[SettingsProvider::kDisplayKey];
    savedDisplay = "B\r\n";
    std::string placed = "?";
    const int& displaySaves = sh.saves[SettingsProvider::kDisplayKey];
    SettingsProvider picker({ .copyLogs = [] { return true; },
                              .listDisplays = [&] { return monitors; },
                              .applyDisplay = [&](const std::string& id) { placed = id; } });
    picker.start(sh);
    expect(sh.published.back().content.expandedCenter.size() == 2 && placed.empty(),
           "display: one monitor, no row; the saved one is unplugged, so the island is on the main display");

    monitors.push_back({ "B", "DELL U2720Q" });
    picker.refreshDisplays();
    expect(placed == "B" && cardText(sh.published.back(), 0) == "Display" && cardItem(sh.published.back(), 0, 1) == "DELL U2720Q",
           "display: the saved monitor plugged back in: the island returns to it");

    auto pressDisplay = [&](const std::string& actionId) { picker.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    pressDisplay(settings_action::kOpenDisplays);
    const Slot& displays = sh.published.back().content.expandedCenter;
    expect(displays.size() == 4 && cardText(sh.published.back(), 1) == "Main display" && cardText(sh.published.back(), 2) == "Home LCD" &&
           std::get<Button>(displays[3].items[1]).glyph == Glyph::Checked && displays[1].list == settings_action::kDisplayList &&
           sh.revealed == std::string(settings_action::kDisplayList) + "@2",
           "display: a page of every choice, the same as the themes', the current one ticked");
    pressDisplay(std::string(settings_action::kPickDisplay) + "0");
    expect(placed.empty() && savedDisplay.empty() && cardItem(sh.published.back(), 0, 1) == "Main display",
           "display: picking Main display moves it there, saved, back to the list");
    pressDisplay(settings_action::kOpenDisplays);
    pressDisplay(std::string(settings_action::kPickDisplay) + "1");
    expect(placed == "A" && savedDisplay == "A" && cardItem(sh.published.back(), 0, 1) == "Home LCD", "display: then the first monitor");
    pressDisplay(settings_action::kOpenDisplays);
    pressDisplay(std::string(settings_action::kPickDisplay) + "2");
    expect(placed == "B" && displaySaves == 3, "display: and the second");

    pressDisplay(settings_action::kOpenDisplays);
    monitors.pop_back(); // B unplugged
    monitors.push_back({ "C", "Projector" });
    picker.refreshDisplays();
    expect(placed.empty() && std::get<Button>(sh.published.back().content.expandedCenter[1].items[1]).glyph == Glyph::Checked &&
           savedDisplay == "B",
           "display: the picked monitor unplugged: main display, but the pick is kept");
    monitors.pop_back();
    picker.refreshDisplays();
    expect(cardText(sh.published.back(), 0) == "Keyboard shortcuts", "display: down to one monitor, the page closes");
    picker.stop();

    // Reopened, the card starts at its list, not the page it was left on: the
    // app tells a provider its card closed when shownCard changes.
    IslandState st;
    ProviderRegistry registry{ [&](const Event& e, const char*) { st = reduce(st, e, Ms{ 0 }); }, [] { return Ms{ 0 }; } };
    registry.add(std::make_unique<SettingsProvider>(SettingsHooks{}));
    registry.start("system.settings");
    st = reduce(st, HomeOpened{}, Ms{ 0 });
    st = reduce(st, TileClicked{ kSettingsPanelId }, Ms{ 0 });
    std::optional<ShownCard> shown = shownCard(st);
    expect(shown && shown->providerId == "system.settings" && shown->activityId == kSettingsPanelId, "reopen: the card shown, and whose");
    auto firstRow = [&] { return findPanel(st, kSettingsPanelId)->content.expandedCenter[0]; };
    registry.route({ "system.settings", kSettingsPanelId, settings_action::kOpenShortcuts, 0 });
    expect(!firstRow().leading.empty(), "reopen: a page open (its title has a back arrow)");
    st = reduce(st, EscapePressed{}, Ms{ 0 });
    expect(!shownCard(st), "reopen: closed, no card shown");
    registry.cardClosed(*shown);
    expect(firstRow().leading.empty() && firstRow().listItem, "reopen: told it closed, the card is back at its list");
    st = reduce(st, HomeOpened{}, Ms{ 0 });
    expect(!shownCard(st), "reopen: Home is no card");
    registry.stop("system.settings");
    setTokens(saved);
}

} // namespace cup::test
