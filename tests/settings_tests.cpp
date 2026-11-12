// Settings (decisions 022, 023, 030, 104): a widget that opens the Settings
// window; its pages, the theme and display pages within Appearance, Copy
// logs, Centre island, Start with Windows, updates. The shortcuts page:
// shortcut_settings_tests.cpp; Tab row, Music colours and "New":
// settings_new_tests.cpp.

#include "expect.h"
#include "panel_host.h"
#include "core/tokens.h"
#include "providers/provider_registry.h"
#include "providers/system/settings_provider.h"

#include <memory>

namespace cup::test {
namespace {

namespace P = settings_action;

Action fromWindow(const std::string& actionId, float value = 0, const std::string& text = "")
{
    return { "system.settings", "", actionId, value, text, kSettingsWindowId };
}

// The ticked choice's name on a page within a page, "" if none.
std::string ticked(const WindowPage* page)
{
    if (!page) return "";
    for (const WindowSection& s : page->sections)
        for (const Row& r : s.rows)
            if (auto* b = itemOf<Button>(&r); b && b->glyph == Glyph::Checked) return nameOf(r);
    return "";
}

void widgetAndThemes()
{
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
    expect(widget.id == SettingsProvider::kPanelId && widget.tapAction == P::kOpenWindow && !hasCard(widget.content) && !sh.window,
           "settings: a widget whose tap opens the window; no card of its own (decision 104)");
    expect(leadGlyph(widget) == Glyph::Settings && textAt(widget, 0, 0) == "Settings" && widget.content.tile.size() == 1 &&
           std::get<Text>(widget.content.tile[0].items[0]).style == TextStyle::Title,
           "settings: the gear in the side square, its name bold like the other widgets' (decision 073)");
    expect(applied == "Lime" && saves == 0, "settings: the saved theme applies at start");

    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(sh.window && sh.window->title == "Settings" && sh.window->pages.size() == 3 && sh.window->pages[0].id == kAppearancePage &&
           sh.window->pages[1].id == kGeneralPage && sh.window->pages[2].id == kUpdatesPage && sh.windowSpec->id == kSettingsWindowId,
           "window: opened by the tap, with Appearance, General and Updates in its sidebar");
    const WindowPage* look = pageOf(sh, kAppearancePage);
    expect(look->title == "Appearance" && !look->subtitle.empty() && look->glyph == Glyph::Palette && look->backAction.empty() &&
           look->sections.size() == 2, "window: Appearance, a line about it, and two groups");
    const Row* themeRow = windowRow(sh, "Theme");
    expect(themeRow && themeRow->clickAnywhere && detailOf(sh, "Theme") == "Lime" && itemOf<Button>(themeRow)->actionId == P::kOpenThemes &&
           itemOf<Button>(themeRow)->glyph == Glyph::ChevronRight,
           "window: the Theme row says the theme under its name; a click anywhere on it opens the themes");

    auto press = [&](const std::string& actionId) { settings.onAction(fromWindow(actionId)); };
    press(P::kOpenThemes);
    look = pageOf(sh, kAppearancePage);
    expect(look->title == "Theme" && look->backAction == P::kBack && look->sections.size() == 2 && look->sections[0].title == "Dark" &&
           look->sections[1].title == "Light",
           "themes: a page within Appearance, with a back arrow: dark ones, then light ones, each under its heading");
    expect(nameOf(look->sections[0].rows[0]) == "Midnight" && nameOf(look->sections[0].rows[1]) == "Grape" &&
           nameOf(look->sections[0].rows[2]) == "Lime" && nameOf(look->sections[1].rows[0]) == "Paper",
           "themes: in tokens.json's order");
    expect(itemOf<Swatch>(&look->sections[0].rows[1]) && *itemOf<Swatch>(&look->sections[0].rows[1]) == Swatch{ .fill = { 24, 9, 42 }, .mark = { 191, 90, 242 } },
           "themes: each starts with a swatch of its island and accent");
    expect(ticked(look) == "Lime", "themes: the picked one ticked");
    press(std::string(P::kPickTheme) + "1");
    expect(applied == "Paper" && stored == "Paper" && saves == 1 && ticked(pageOf(sh, kAppearancePage)) == "Paper",
           "themes: a pick applies and saves, the page stays, the tick moved: the island and the window are the preview");
    press(P::kBack);
    expect(pageOf(sh, kAppearancePage)->title == "Appearance" && detailOf(sh, "Theme") == "Paper", "themes: back shows Appearance, with the pick");
    press(P::kOpenThemes);
    press(P::kBack);
    press(std::string(P::kPickTheme) + "9");
    expect(applied == "Paper" && saves == 1 && detailOf(sh, "Theme") == "Paper", "themes: back changes nothing; a choice that's gone is ignored");
    settings.onAction({ "system.settings", "elsewhere", P::kCopyLogs, 0 });
    expect(copies == 0, "window: an action from anywhere but its window (or the widget's tap) does nothing");

    stored = "Renamed";
    settings.stop();
    expect(!sh.window, "settings: stopping closes the window");
    settings.start(sh);
    expect(applied == "Midnight", "settings: a saved theme tokens.json no longer has falls back to the default");
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    themed.themes.erase(themed.themes.begin());
    setTokens(themed);
    settings.refresh();
    expect(applied == "Paper" && detailOf(sh, "Theme") == "Paper", "settings: a reload that drops the theme moves to the new first");

    // Copy logs (General): says how the copy went, then resets.
    expect(windowRow(sh, "Copy logs") && !sh.wake, "logs: idle, nothing scheduled");
    sh.clock = Ms{ 1000 };
    press(P::kCopyLogs);
    expect(copies == 1 && windowRow(sh, "Logs copied"), "logs: a click copies and says so");
    expect(sh.wake == Ms{ 1000 } + tokens().logsNote, "logs: the note lasts logsNoteMs");
    settings.onWake();
    expect(windowRow(sh, "Copy logs"), "logs: then back to Copy logs");
    press(P::kCentreIsland);
    const Row* centreRow = windowRow(sh, "Centre island");
    expect(centred == 1 && centreRow && centreRow->clickAnywhere && itemOf<Button>(centreRow)->glyph == Glyph::Centre &&
           itemOf<Button>(windowRow(sh, "Copy logs"))->glyph == Glyph::Copy,
           "settings: Copy logs and Centre island are rows like the others; Centre puts a moved island back (decision 026)");
    copyWorks = false;
    press(P::kCopyLogs);
    expect(windowRow(sh, "Couldn't copy logs"), "logs: a failed copy says so");
    settings.stop();
    expect(sh.removed.back() == SettingsProvider::kPanelId && !sh.wake, "settings: stop removes the widget and the wake-up");

    themed.themes.clear();
    setTokens(themed);
    settings.start(sh);
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(!windowRow(sh, "Theme") && windowRow(sh, "Centre island"), "settings: no themes in tokens.json, no theme row");
    settings.stop();
    setTokens(saved);
}

void updatesAndVersion()
{
    // The one updates row (decisions 036, 091): it checks, and says what it found under its name.
    PanelHost uh;
    std::function<void(CheckOutcome, const std::string&)> pending;
    int checks = 0;
    SettingsProvider s({ .version = "0.4.0", .checkForUpdates = [&](std::function<void(CheckOutcome, const std::string&)> reply) {
        ++checks;
        pending = std::move(reply);
    } });
    s.start(uh);
    s.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    auto said = [&] { return detailOf(uh, "Check for updates"); };
    const Row* check = windowRow(uh, "Check for updates");
    expect(check && check->clickAnywhere && said() == "Version 0.4.0" && itemOf<Button>(check)->glyph == Glyph::Refresh &&
           pageOf(uh, kUpdatesPage)->sections[0].rows.size() == 1,
           "updates: one row on Updates, the running version under it until a check, a refresh glyph");
    auto press = [&] { s.onAction(fromWindow(P::kCheckUpdates)); };
    press();
    expect(checks == 1 && said() == "Checking…" && !itemOf<Button>(windowRow(uh, "Check for updates"))->enabled, "updates: checking");
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
    s.onWindowClosed(kSettingsWindowId);
    s.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(said() == "Version 0.4.0", "updates: reopened, the row starts fresh");
    s.stop();

    // Update and restart (decisions 037, 091): an offer turns the same row into it.
    PanelHost ih;
    int installs = 0;
    SettingsProvider offered({ .version = "0.5.0", .checkForUpdates = [](auto) {}, .installUpdate = [&] { ++installs; } });
    offered.start(ih);
    offered.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(windowRow(ih, "Check for updates") && !windowRow(ih, "Update and restart"), "install: nothing offered, the row checks");
    offered.updateStatusChanged(UpdateStatus{ .version = "0.5.1" });
    const Row* install = windowRow(ih, "Update and restart");
    expect(install && !windowRow(ih, "Check for updates") && detailOf(ih, "Update and restart") == "0.5.0 → 0.5.1" && install->clickAnywhere &&
           itemOf<Button>(install)->glyph == Glyph::Download && itemOf<Button>(install)->enabled && pageOf(ih, kUpdatesPage)->sections[0].rows.size() == 1,
           "install: an offer turns the same row into Update and restart, both versions under it");
    expect(!ih.published.back().content.badge && !itemOf<Button>(install)->badge, "install: no dot in Settings: the Update widget has it (decision 038)");
    offered.onAction(fromWindow(P::kInstallUpdate));
    expect(installs == 1, "install: a click installs it");
    offered.updateStatusChanged(UpdateStatus{ .version = "0.5.1", .stage = UpdateStage::Installing });
    expect(detailOf(ih, "Update and restart") == "Installing…" && !itemOf<Button>(windowRow(ih, "Update and restart"))->enabled, "install: under way, it says so");
    offered.onAction(fromWindow(P::kInstallUpdate));
    expect(installs == 1, "install: a second click while installing does nothing");
    offered.updateStatusChanged(UpdateStatus{ .version = "0.5.1", .stage = UpdateStage::Failed, .problem = "Couldn't download" });
    expect(detailOf(ih, "Try the update again") == "Couldn't download", "install: a failure says why, and offers again");
    offered.updateStatusChanged(std::nullopt);
    expect(windowRow(ih, "Check for updates") && !windowRow(ih, "Try the update again"), "install: withdrawn, the row checks again");
    offered.stop();

    // No way to check: no row, but the version still shows (decision 036).
    PanelHost vh;
    SettingsProvider versioned({ .version = "0.4.0" });
    versioned.start(vh);
    versioned.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(!windowRow(vh, "Check for updates") && detailOf(vh, "Version") == "Cup Island 0.4.0", "updates: no way to check: the version, in a row");
    versioned.stop();
}

void startupAndDisplays()
{
    // Start with Windows (decision 030): on the first time, then as the user leaves it.
    PanelHost sh;
    bool autostart = false;
    int autostartWrites = 0;
    std::string& savedAutostart = sh.store[SettingsProvider::kAutostartKey];
    SettingsProvider startup({ .startsWithWindows = [&] { return autostart; },
                               .setStartsWithWindows = [&](bool on) { autostart = on; ++autostartWrites; } });
    startup.start(sh);
    startup.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    const Row* row = windowRow(sh, "Start with Windows");
    expect(autostart && savedAutostart == "on" && itemOf<Switch>(row)->on && row->clickAnywhere && pageOf(sh, kGeneralPage)->sections[0].rows[0].id == "autostart",
           "startup: on by default, its switch on, first on General");
    startup.onAction(fromWindow(P::kToggleAutostart));
    expect(!autostart && savedAutostart == "off" && !itemOf<Switch>(windowRow(sh, "Start with Windows"))->on, "startup: a click turns it off, saved");
    startup.stop();
    startup.start(sh);
    expect(!autostart && autostartWrites == 2, "startup: off stays off after a restart");
    startup.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    autostart = true; // turned on elsewhere
    startup.onAction(fromWindow(P::kToggleAutostart));
    expect(!autostart && savedAutostart == "off", "startup: the row reads the real state, not the saved one");
    startup.stop();

    // Display picker (decision 023): "Main display", then each monitor, by name.
    PanelHost dh;
    std::vector<DisplayChoice> monitors{ { "A", "Home LCD" } };
    std::string& savedDisplay = dh.store[SettingsProvider::kDisplayKey];
    savedDisplay = "B\r\n";
    std::string placed = "?";
    const int& displaySaves = dh.saves[SettingsProvider::kDisplayKey];
    SettingsProvider picker({ .copyLogs = [] { return true; },
                              .listDisplays = [&] { return monitors; },
                              .applyDisplay = [&](const std::string& id) { placed = id; } });
    picker.start(dh);
    picker.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(!windowRow(dh, "Display") && placed.empty(), "display: one monitor, no row; the saved one is unplugged, so the island is on the main display");
    monitors.push_back({ "B", "DELL U2720Q" });
    picker.refreshDisplays();
    expect(placed == "B" && detailOf(dh, "Display") == "DELL U2720Q", "display: the saved monitor plugged back in: the island returns to it");
    auto press = [&](const std::string& actionId) { picker.onAction(fromWindow(actionId)); };
    press(P::kOpenDisplays);
    const WindowPage* displays = pageOf(dh, kAppearancePage);
    expect(displays->title == "Display" && displays->backAction == P::kBack && displays->sections[0].rows.size() == 3 &&
           nameOf(displays->sections[0].rows[0]) == "Main display" && nameOf(displays->sections[0].rows[1]) == "Home LCD" && ticked(displays) == "DELL U2720Q",
           "display: a page of every choice, the same as the themes', the current one ticked");
    press(std::string(P::kPickDisplay) + "0");
    expect(placed.empty() && savedDisplay.empty() && detailOf(dh, "Display") == "Main display", "display: picking Main display moves it there, saved, back to Appearance");
    press(P::kOpenDisplays);
    press(std::string(P::kPickDisplay) + "1");
    expect(placed == "A" && savedDisplay == "A" && detailOf(dh, "Display") == "Home LCD", "display: then the first monitor");
    press(P::kOpenDisplays);
    press(std::string(P::kPickDisplay) + "2");
    expect(placed == "B" && displaySaves == 3, "display: and the second");
    press(P::kOpenDisplays);
    monitors.pop_back(); // B unplugged
    monitors.push_back({ "C", "Projector" });
    picker.refreshDisplays();
    expect(placed.empty() && ticked(pageOf(dh, kAppearancePage)) == "Main display" && savedDisplay == "B",
           "display: the picked monitor unplugged: main display, but the pick is kept");
    monitors.pop_back();
    picker.refreshDisplays();
    expect(pageOf(dh, kAppearancePage)->title == "Appearance" && !windowRow(dh, "Display"), "display: down to one monitor, the page closes");
    picker.stop();
}

// End to end through the registry: the widget's tap is the user acting on
// Settings, so its window may open (decision 104); closing it resets it.
void throughTheRegistry()
{
    IslandState st;
    ProviderRegistry registry{ [&](const Event& e, const char*) { st = reduce(st, e, Ms{ 0 }); }, [] { return Ms{ 0 }; } };
    registry.add(std::make_unique<SettingsProvider>(SettingsHooks{}));
    registry.start("system.settings");
    const Panel tile = *findPanel(st, kSettingsPanelId); // a copy: the state is replaced as events arrive
    const Panel* widget = &tile;
    registry.route({ widget->providerId, widget->id, widget->tapAction, 0 }); // as the app sends a tile's tap
    const OpenWindow* open = findWindow(registry.windows(), { "system.settings", kSettingsWindowId });
    expect(open && open->page == kAppearancePage && open->content.pages.size() == 3, "registry: the tap opens the window on Appearance");
    registry.windowPageShown({ "system.settings", kSettingsWindowId }, kGeneralPage);
    registry.route(fromWindow(P::kOpenShortcuts));
    open = findWindow(registry.windows(), { "system.settings", kSettingsWindowId });
    expect(open->page == kGeneralPage && open->content.pages[1].title == "Keyboard shortcuts", "registry: General picked, its shortcuts opened");
    registry.windowPageShown({ "system.settings", kSettingsWindowId }, kUpdatesPage);
    registry.windowPageShown({ "system.settings", kSettingsWindowId }, kGeneralPage);
    open = findWindow(registry.windows(), { "system.settings", kSettingsWindowId });
    expect(open->content.pages[1].title == "General", "registry: another page in the sidebar, then back: General at its settings");
    registry.route(fromWindow(P::kOpenShortcuts));
    registry.windowClosed({ "system.settings", kSettingsWindowId });
    expect(registry.windows().open.empty(), "registry: the user closes it");
    registry.route({ widget->providerId, widget->id, widget->tapAction, 0 });
    open = findWindow(registry.windows(), { "system.settings", kSettingsWindowId });
    expect(open && open->content.pages[1].title == "General" && open->page == kAppearancePage, "registry: reopened, it starts at its pages again");
    registry.stop("system.settings");
    expect(registry.windows().open.empty(), "registry: Settings stopping takes its window");
}

} // namespace

void runSettingsTests()
{
    widgetAndThemes();
    updatesAndVersion();
    startupAndDisplays();
    throughTheRegistry();
}

} // namespace cup::test
