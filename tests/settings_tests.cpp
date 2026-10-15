// The Settings widget (decisions 022, 023, 030): its list of settings, the
// theme and display pages, Copy logs, Centre island, Start with Windows.
// Its keyboard shortcuts page: shortcut_settings_tests.cpp.

#include "expect.h"
#include "panel_host.h"
#include "core/layout.h"
#include "core/slot_layout.h"
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
    expect(applied == "Lime" && saves == 0 && valueOf(widget, "Theme") == "Lime",
           "settings: the saved theme applies at start, and its row says so");
    const Row& themeRow = *rowNamed(widget, "Theme");
    const Row& first = widget.content.expandedCenter[0];
    expect(std::get<Text>(first.items[0]).value == "Appearance" && std::get<Text>(first.items[0]).style == TextStyle::Heading &&
           rowNamed(widget, "General") && rowNamed(widget, "General")->section && themeRow.list == settings_action::kSettingsList &&
           themeRow.visibleRows == tokens().listCardVisibleRows,
           "settings: the list in groups under headings, one list sized to the card (decision 037)");
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
    expect(std::get<Text>(page[1].items[0]).style == TextStyle::Heading && std::get<Text>(page[5].items[0]).style == TextStyle::Heading &&
           !page[1].section && page[5].section && page[5].inset && !page[5].listItem,
           "themes: the headings stand out (Heading), the second with room above it, lined up with the names (decision 037)");
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
    expect(valueOf(sh.published.back(), "Theme") == "Paper", "themes: back shows the list, with the pick");
    press(settings_action::kOpenThemes);
    press(settings_action::kBack);
    press(std::string(settings_action::kPickTheme) + "9");
    expect(applied == "Paper" && saves == 1 && valueOf(sh.published.back(), "Theme") == "Paper",
           "themes: back changes nothing; a choice that's gone is ignored");

    stored = "Renamed";
    settings.stop();
    settings.start(sh);
    expect(applied == "Midnight", "settings: a saved theme tokens.json no longer has falls back to the default");
    themed.themes.erase(themed.themes.begin());
    setTokens(themed);
    settings.refresh();
    expect(applied == "Paper" && valueOf(sh.published.back(), "Theme") == "Paper", "settings: a reload that drops the theme moves to the new first");


    // Copy logs moved here from its own tile: says how the copy went, then resets.
    expect(rowNamed(sh.published.back(), "Copy logs") && !sh.wake, "logs: idle, nothing scheduled");
    sh.clock = Ms{ 1000 };
    press(settings_action::kCopyLogs);
    expect(copies == 1 && rowNamed(sh.published.back(), "Logs copied"), "logs: a click copies and says so");
    expect(sh.wake == Ms{ 1000 } + tokens().logsNote, "logs: the note lasts logsNoteMs");
    settings.onWake();
    expect(rowNamed(sh.published.back(), "Copy logs"), "logs: then back to Copy logs");
    press(settings_action::kCentreIsland);
    const Row* centreRow = rowNamed(sh.published.back(), "Centre island");
    const Row* logsRow = rowNamed(sh.published.back(), "Copy logs");
    expect(centred == 1 && centreRow && centreRow->listItem && centreRow->clickAnywhere &&
           std::get<Button>(centreRow->items[1]).glyph == Glyph::Centre && logsRow->listItem &&
           std::get<Button>(logsRow->items[1]).glyph == Glyph::Copy,
           "settings: Copy logs and Centre island are list rows like the others; Centre puts a moved island back (decision 026)");
    copyWorks = false;
    press(settings_action::kCopyLogs);
    expect(rowNamed(sh.published.back(), "Couldn't copy logs"), "logs: a failed copy says so");
    settings.stop();
    expect(sh.removed.back() == SettingsProvider::kPanelId && !sh.wake, "settings: stop removes the widget and the wake-up");

    themed.themes.clear();
    setTokens(themed);
    settings.start(sh);
    expect(!rowNamed(sh.published.back(), "Theme") && rowNamed(sh.published.back(), "Centre island"),
           "settings: no themes in tokens.json, no theme row");
    settings.stop();

    // Check for updates (decision 036): a row that checks and says what it found.
    {
        PanelHost uh;
        std::function<void(CheckOutcome, const std::string&)> pending;
        int checks = 0;
        SettingsProvider s({ .checkForUpdates = [&](std::function<void(CheckOutcome, const std::string&)> reply) {
            ++checks;
            pending = std::move(reply);
        } });
        s.start(uh);
        auto updateRow = [&]() -> const Row* {
            for (const Row& r : uh.published.back().content.expandedCenter)
                if (!r.items.empty() && std::holds_alternative<Text>(r.items[0]) && std::get<Text>(r.items[0]).value == "Check for updates") return &r;
            return nullptr;
        };
        auto said = [&] { return std::get<Text>(updateRow()->items[1]).value; };
        expect(updateRow() && updateRow()->listItem && updateRow()->clickAnywhere && said().empty(), "updates: a row that says nothing yet");
        auto press = [&] { s.onAction({ "system.settings", SettingsProvider::kPanelId, settings_action::kCheckUpdates, 0 }); };
        press();
        expect(checks == 1 && said() == "Checking…" && !std::get<Button>(updateRow()->items[2]).enabled, "updates: checking");
        press();
        expect(checks == 1, "updates: a second click while checking does nothing");
        pending(CheckOutcome::Available, "0.5.1");
        expect(said() == "0.5.1 available", "updates: found");
        press();
        pending(CheckOutcome::UpToDate, "");
        expect(said() == "Up to date", "updates: up to date");
        press();
        pending(CheckOutcome::Failed, "");
        expect(said() == "Couldn't check", "updates: offline");
        s.onCardClosed(SettingsProvider::kPanelId);
        expect(said().empty(), "updates: reopened, the row starts fresh");
        s.stop();

        // Update and restart (decision 037): under Check for updates while an
        // update is offered, whichever check found it; the same install as Home's card.
        PanelHost ih;
        int installs = 0;
        SettingsProvider offered({ .checkForUpdates = [](auto) {}, .installUpdate = [&] { ++installs; } });
        offered.start(ih);
        auto rowAfterCheck = [&]() -> std::optional<Row> {
            const Slot& card = ih.published.back().content.expandedCenter;
            for (std::size_t i = 0; i + 1 < card.size(); ++i)
                if (std::get<Text>(card[i].items[0]).value == "Check for updates") return card[i + 1];
            return std::nullopt;
        };
        auto textOf = [](const Row& r, std::size_t i) { return std::get<Text>(r.items[i]).value; };
        expect(!rowAfterCheck(), "install: nothing offered, no row (Updates is the last group)");
        offered.updateStatusChanged(UpdateStatus{ .version = "0.5.1" });
        auto install = rowAfterCheck();
        expect(install && textOf(*install, 0) == "Update and restart" && textOf(*install, 1) == "0.5.1" && install->listItem &&
               install->clickAnywhere && std::get<Button>(install->items[2]).enabled,
               "install: an offer (from the daily check too) puts Update and restart under Check for updates");
        expect(!ih.published.back().content.badge && !std::get<Button>(install->items[2]).badge,
               "install: no dot in Settings: the Update widget has it (decision 038)");
        offered.onAction({ "system.settings", SettingsProvider::kPanelId, settings_action::kInstallUpdate, 0 });
        expect(installs == 1, "install: a click installs it");
        offered.updateStatusChanged(UpdateStatus{ .version = "0.5.1", .stage = UpdateStage::Installing });
        install = rowAfterCheck();
        expect(textOf(*install, 1) == "Installing…" && !std::get<Button>(install->items[2]).enabled, "install: under way, it says so");
        offered.onAction({ "system.settings", SettingsProvider::kPanelId, settings_action::kInstallUpdate, 0 });
        expect(installs == 1, "install: a second click while installing does nothing");
        offered.updateStatusChanged(UpdateStatus{ .version = "0.5.1", .stage = UpdateStage::Failed, .problem = "Couldn't download" });
        install = rowAfterCheck();
        expect(textOf(*install, 0) == "Try the update again" && textOf(*install, 1) == "Couldn't download", "install: a failure says why, and offers again");
        offered.updateStatusChanged(std::nullopt);
        expect(!rowAfterCheck(), "install: withdrawn, the row goes");
        offered.stop();

        PanelHost nh;
        SettingsProvider none({});
        none.start(nh);
        bool any = false;
        for (const Row& r : nh.published.back().content.expandedCenter)
            any = any || (!r.items.empty() && std::holds_alternative<Text>(r.items[0]) && std::get<Text>(r.items[0]).value == "Check for updates");
        expect(!any, "updates: no way to check, no row");
        none.stop();
    }

    // The running version, under the list (decision 036).
    {
        PanelHost vh;
        SettingsProvider versioned({ .version = "0.4.0" });
        versioned.start(vh);
        const Slot& card = vh.published.back().content.expandedCenter;
        expect(cardText(vh.published.back(), card.size() - 1) == "Cup Island 0.4.0", "settings: the version, last");
        versioned.stop();
    }

    // Start with Windows (decision 030): on the first time, then as the user leaves it.
    bool autostart = false;
    int autostartWrites = 0;
    std::string& savedAutostart = sh.store[SettingsProvider::kAutostartKey];
    SettingsProvider startup({ .startsWithWindows = [&] { return autostart; },
                               .setStartsWithWindows = [&](bool on) { autostart = on; ++autostartWrites; } });
    startup.start(sh);
    const Row& startupRow = *rowNamed(sh.published.back(), "Start with Windows");
    expect(autostart && savedAutostart == "on" &&
           std::get<Switch>(startupRow.items[1]).on && startupRow.clickAnywhere,
           "startup: on by default, ticked");
    auto pressStartup = [&](const char* actionId) { startup.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    pressStartup(settings_action::kToggleAutostart);
    expect(!autostart && savedAutostart == "off" &&
           !std::get<Switch>(rowNamed(sh.published.back(), "Start with Windows")->items[1]).on,
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
    expect(!rowNamed(sh.published.back(), "Display") && placed.empty(),
           "display: one monitor, no row; the saved one is unplugged, so the island is on the main display");

    monitors.push_back({ "B", "DELL U2720Q" });
    picker.refreshDisplays();
    expect(placed == "B" && valueOf(sh.published.back(), "Display") == "DELL U2720Q",
           "display: the saved monitor plugged back in: the island returns to it");

    auto pressDisplay = [&](const std::string& actionId) { picker.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    pressDisplay(settings_action::kOpenDisplays);
    const Slot& displays = sh.published.back().content.expandedCenter;
    expect(displays.size() == 4 && cardText(sh.published.back(), 1) == "Main display" && cardText(sh.published.back(), 2) == "Home LCD" &&
           std::get<Button>(displays[3].items[1]).glyph == Glyph::Checked && displays[1].list == settings_action::kDisplayList &&
           sh.revealed == std::string(settings_action::kDisplayList) + "@2",
           "display: a page of every choice, the same as the themes', the current one ticked");
    pressDisplay(std::string(settings_action::kPickDisplay) + "0");
    expect(placed.empty() && savedDisplay.empty() && valueOf(sh.published.back(), "Display") == "Main display",
           "display: picking Main display moves it there, saved, back to the list");
    pressDisplay(settings_action::kOpenDisplays);
    pressDisplay(std::string(settings_action::kPickDisplay) + "1");
    expect(placed == "A" && savedDisplay == "A" && valueOf(sh.published.back(), "Display") == "Home LCD", "display: then the first monitor");
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
    expect(rowNamed(sh.published.back(), "Keyboard shortcuts") && !rowNamed(sh.published.back(), "Display"),
           "display: down to one monitor, the page closes");
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
    expect(firstRow().leading.empty() && rowNamed(*findPanel(st, kSettingsPanelId), "Copy logs"), "reopen: told it closed, the card is back at its list");
    st = reduce(st, HomeOpened{}, Ms{ 0 });
    expect(!shownCard(st), "reopen: Home is no card");
    registry.stop("system.settings");

    // Every row there (two monitors, an update offered, with tabs): the root
    // page scrolls instead of growing past the canvas (decision 037).
    Tokens withThemes = saved;
    withThemes.themes = { { "Midnight", {} }, { "Grape", {} } }; // a Theme row too
    setTokens(withThemes);
    Panel full = settingsPanel({ .theme = "Midnight", .displays = { kMainDisplayName, "DELL U2720Q" }, .startsWithWindows = true,
                                 .version = "5.2.0", .update = UpdateNote{}, .offer = UpdateStatus{ .version = "5.3.0" } });
    full.providerId = "system.settings";
    IslandState big;
    big = reduce(big, PanelPublished{ full }, Ms{ 0 });
    big = reduce(big, ActivityStarted{ { .id = "media", .content = { .label = "m", .compactLeading = { row({ Icon{} }) } } } }, Ms{ 0 });
    big = reduce(big, HomeOpened{}, Ms{ 0 });
    big = reduce(big, TileClicked{ kSettingsPanelId }, Ms{ 0 });
    Scene scene = layout(big, Ms{ 0 });
    const Tokens& t = tokens();
    float pad = t.expandedPadding + t.listCardVerticalInset;
    float needed = slotHeight(full.content.expandedCenter, { 0, 0, t.listCardWidth, t.canvas.y }) + 2 * pad;
    const Placed* lastRow = nullptr;
    for (const Placed& e : scene.elements)
        if (e.list && e.list->id == settings_action::kSettingsList) lastRow = &e;
    expect(rowNamed(full, "Theme") && rowNamed(full, "Display") && rowNamed(full, "Update and restart") && scene.tabs && needed <= t.canvas.y - (t.tabHeight + t.slotGap) && lastRow && lastRow->list->max > 0,
           "settings: with everything, the list scrolls and the card still fits the canvas");
    expect(valueOf(full, "Check for updates") == "5.2.0" && !rowNamed(full, "Cup Island 5.2.0"),
           "settings: the version is Check for updates' value until a check, not a row of its own");
    setTokens(saved);
}

} // namespace cup::test
