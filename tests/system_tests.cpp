// Home's system widgets (decision 009): which Host calls each theme and
// Bluetooth reading produces, and what the tiles say. Plus the Settings widget.

#include "expect.h"
#include "core/tokens.h"
#include "providers/system/settings_provider.h"
#include "providers/system/system_panels.h"

#include <map>
#include <vector>

namespace cup::test {
namespace {

struct PanelHost : Host {
    std::vector<Panel> published;
    std::vector<std::string> removed;

    void startActivity(Activity) override {}
    void endActivity(const std::string&) override {}
    void postAlert(Alert) override {}
    Ms clock{ 0 };
    std::optional<Ms> wake;
    Ms now() const override { return clock; }
    void publishPanel(Panel p) override { published.push_back(std::move(p)); }
    void removePanel(const std::string& id) override { removed.push_back(id); }
    void wakeAt(std::optional<Ms> at) override { wake = at; }
    void ask(Dialog) override {}
    std::string revealed; // "<list>@<row>"
    void reveal(const std::string&, const std::string& list, std::size_t row) override { revealed = list + "@" + std::to_string(row); }
    std::map<std::string, std::string> store; // Host::load/save
    std::map<std::string, int> saves;         // per key
    std::string load(const std::string& key) const override { auto it = store.find(key); return it == store.end() ? "" : it->second; }
    void save(const std::string& key, const std::string& text) override { store[key] = text; ++saves[key]; }
    Ms wallClock() const override { return clock; }
};

std::string textAt(const Panel& p, std::size_t row, std::size_t item)
{
    if (row >= p.content.tile.size() || item >= p.content.tile[row].items.size()) return "?";
    auto* text = std::get_if<Text>(&p.content.tile[row].items[item]);
    return text ? text->value : "?";
}

// A card row's text, or its button's label.
std::string cardText(const Panel& p, std::size_t row)
{
    const Slot& card = p.content.expandedCenter;
    if (row >= card.size() || card[row].items.empty()) return "?";
    if (auto* text = std::get_if<Text>(&card[row].items[0])) return text->value;
    if (auto* button = std::get_if<Button>(&card[row].items[0])) return button->label;
    return "?";
}

// A card row's `item`th item's text.
std::string cardItem(const Panel& p, std::size_t row, std::size_t item)
{
    const Slot& card = p.content.expandedCenter;
    if (row >= card.size() || item >= card[row].items.size()) return "?";
    auto* text = std::get_if<Text>(&card[row].items[item]);
    return text ? text->value : "?";
}

Glyph glyphAt(const Panel& p, std::size_t row)
{
    auto* icon = std::get_if<Icon>(&p.content.tile[row].items[0]);
    return icon ? icon->glyph : Glyph::Warning;
}

} // namespace

void runSystemTests()
{
    // Theme: a toggle tile whose tap switches the mode; republished on change only.
    PanelHost th;
    ThemePresenter theme(th);
    theme.show(std::nullopt);
    expect(th.published.empty() && th.removed.empty(), "can't tell the mode: no tile");
    theme.show(true);
    const Panel& dark = th.published.back();
    expect(dark.id == ThemePresenter::kPanelId && dark.tapAction == ThemePresenter::kToggle && !hasCard(dark.content),
           "a tap-action tile, no card");
    expect(glyphAt(dark, 0) == Glyph::Moon && textAt(dark, 1, 0) == "Dark", "dark: the moon");
    theme.show(true);
    expect(th.published.size() == 1, "same mode: no republish");
    theme.show(false);
    expect(th.published.size() == 2 && glyphAt(th.published.back(), 0) == Glyph::Sun && textAt(th.published.back(), 1, 0) == "Light",
           "light: the sun");

    // Bluetooth: the connected device, preferring one that reports its battery.
    PanelHost bt;
    BluetoothPresenter bluetooth(bt);
    bluetooth.show({});
    expect(bt.published.empty() && bt.removed.empty(), "nothing connected, nothing shown");
    bluetooth.show({ { "kb", "Keyboard", std::nullopt } });
    expect(bt.published.size() == 1 && bt.published[0].wide && !hasCard(bt.published[0].content), "a wide static widget");
    expect(textAt(bt.published[0], 0, 1) == "Keyboard" && textAt(bt.published[0], 1, 0) == "Connected", "no battery: just connected");
    bluetooth.show({ { "kb", "Keyboard", std::nullopt }, { "hp", "WH-1000XM4", 70 } });
    const Panel& two = bt.published.back();
    expect(textAt(two, 0, 1) == "WH-1000XM4" && textAt(two, 1, 0) == "70%", "the device with a battery wins");
    expect(textAt(two, 1, 1) == "+1 more", "the others are counted");
    std::size_t before = bt.published.size();
    bluetooth.show({ { "kb", "Keyboard", std::nullopt }, { "hp", "WH-1000XM4", 70 } });
    expect(bt.published.size() == before, "same devices: no republish");
    bluetooth.show({ { "hp", "WH-1000XM4", 65 } });
    expect(textAt(bt.published.back(), 1, 0) == "65%" && bt.published.back().content.tile[1].items.size() == 1,
           "battery drops, the count goes");
    bluetooth.show({});
    expect(bt.removed.size() == 1 && bt.removed[0] == BluetoothPresenter::kPanelId, "disconnected: panel removed");
    bluetooth.show({});
    expect(bt.removed.size() == 1, "and removed once");

    // Settings (decisions 022, 030): a widget whose card is a list of settings.
    Tokens saved = tokens();
    Tokens themed = saved;
    themed.themes = { { "Midnight", {} }, { "Paper", {}, true }, { "Grape", { { "islandColor", { 24, 9, 42 } } } }, { "Lime", {} } };
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
           widget.content.card == CardLayout::Centred && widget.content.keepOpen,
           "settings: a widget that opens a centred card, which a click between rows doesn't close");
    expect(glyphAt(widget, 0) == Glyph::Settings && textAt(widget, 1, 0) == "Settings", "settings: the gear");
    expect(applied == "Lime" && saves == 0 && cardText(widget, 0) == "Theme" && cardItem(widget, 0, 1) == "Lime",
           "settings: the saved theme applies at start, and its row says so");
    const Row& themeRow = widget.content.expandedCenter[0];
    expect(themeRow.listItem && themeRow.clickAnywhere && std::get<Button>(themeRow.items[2]).actionId == SettingsProvider::kOpenThemes,
           "settings: the theme row is a list item; a click anywhere on it opens the themes");

    auto press = [&](const std::string& actionId) { settings.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    press(SettingsProvider::kOpenThemes);
    const Slot& page = sh.published.back().content.expandedCenter;
    expect(page.size() == 7 && cardText(sh.published.back(), 0) == "Theme" &&
           std::get<Button>(page[0].leading[0]).actionId == SettingsProvider::kBack, "themes: a page with a back arrow");
    expect(cardText(sh.published.back(), 1) == "Dark" && cardText(sh.published.back(), 2) == "Midnight" &&
           cardText(sh.published.back(), 3) == "Grape" && cardText(sh.published.back(), 4) == "Lime" &&
           cardText(sh.published.back(), 5) == "Light" && cardText(sh.published.back(), 6) == "Paper",
           "themes: dark ones, then light ones, each under its heading");
    expect(std::get<Button>(page[4].items[1]).glyph == Glyph::Checked && std::get<Button>(page[2].items[1]).glyph == Glyph::Unchecked &&
           page[4].list == SettingsProvider::kThemeList && page[1].list == SettingsProvider::kThemeList,
           "themes: one scrolling list, the picked one ticked");
    expect(sh.revealed == std::string(SettingsProvider::kThemeList) + "@3", "themes: scrolled to the picked one");
    press(std::string(SettingsProvider::kPickTheme) + "1");
    expect(applied == "Paper" && stored == "Paper" && saves == 1 && cardText(sh.published.back(), 0) == "Theme" &&
           cardItem(sh.published.back(), 0, 1) == "Paper", "themes: a pick applies, saves and goes back to the list");
    press(SettingsProvider::kOpenThemes);
    press(SettingsProvider::kBack);
    press(std::string(SettingsProvider::kPickTheme) + "9");
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
    expect(cardText(sh.published.back(), 1) == "Copy logs" && !sh.wake, "logs: idle, nothing scheduled");
    sh.clock = Ms{ 1000 };
    press(SettingsProvider::kCopyLogs);
    expect(copies == 1 && cardText(sh.published.back(), 1) == "Logs copied", "logs: a click copies and says so");
    expect(sh.wake == Ms{ 1000 } + tokens().logsNote, "logs: the note lasts logsNoteMs");
    settings.onWake();
    expect(cardText(sh.published.back(), 1) == "Copy logs", "logs: then back to Copy logs");
    press(SettingsProvider::kCentreIsland);
    const Row& buttons = sh.published.back().content.expandedCenter[1];
    expect(centred == 1 && buttons.items.size() == 2 && std::get<Button>(buttons.items[1]).label == "Centre island",
           "settings: Centre island beside Copy logs puts a moved island back (decision 026)");
    copyWorks = false;
    press(SettingsProvider::kCopyLogs);
    expect(cardText(sh.published.back(), 1) == "Couldn't copy logs", "logs: a failed copy says so");
    settings.stop();
    expect(sh.removed.back() == SettingsProvider::kPanelId && !sh.wake, "settings: stop removes the widget and the wake-up");

    themed.themes.clear();
    setTokens(themed);
    settings.start(sh);
    expect(sh.published.back().content.expandedCenter.size() == 1 && cardText(sh.published.back(), 0) == "Copy logs",
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
    pressStartup(SettingsProvider::kToggleAutostart);
    expect(!autostart && savedAutostart == "off" &&
           std::get<Button>(sh.published.back().content.expandedCenter[0].items[1]).glyph == Glyph::Unchecked,
           "startup: a click turns it off, saved");
    startup.stop();
    startup.start(sh);
    expect(!autostart && autostartWrites == 2, "startup: off stays off after a restart");
    autostart = true; // turned on elsewhere
    pressStartup(SettingsProvider::kToggleAutostart);
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
    expect(sh.published.back().content.expandedCenter.size() == 1 && placed.empty(),
           "display: one monitor, no row; the saved one is unplugged, so the island is on the main display");

    monitors.push_back({ "B", "DELL U2720Q" });
    picker.refreshDisplays();
    expect(placed == "B" && cardText(sh.published.back(), 0) == "Display" && cardItem(sh.published.back(), 0, 1) == "DELL U2720Q",
           "display: the saved monitor plugged back in: the island returns to it");

    auto pressDisplay = [&](const std::string& actionId) { picker.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    pressDisplay(SettingsProvider::kOpenDisplays);
    const Slot& displays = sh.published.back().content.expandedCenter;
    expect(displays.size() == 4 && cardText(sh.published.back(), 1) == "Main display" && cardText(sh.published.back(), 2) == "Home LCD" &&
           std::get<Button>(displays[3].items[1]).glyph == Glyph::Checked && displays[1].list == SettingsProvider::kDisplayList &&
           sh.revealed == std::string(SettingsProvider::kDisplayList) + "@2",
           "display: a page of every choice, the same as the themes', the current one ticked");
    pressDisplay(std::string(SettingsProvider::kPickDisplay) + "0");
    expect(placed.empty() && savedDisplay.empty() && cardItem(sh.published.back(), 0, 1) == "Main display",
           "display: picking Main display moves it there, saved, back to the list");
    pressDisplay(SettingsProvider::kOpenDisplays);
    pressDisplay(std::string(SettingsProvider::kPickDisplay) + "1");
    expect(placed == "A" && savedDisplay == "A" && cardItem(sh.published.back(), 0, 1) == "Home LCD", "display: then the first monitor");
    pressDisplay(SettingsProvider::kOpenDisplays);
    pressDisplay(std::string(SettingsProvider::kPickDisplay) + "2");
    expect(placed == "B" && displaySaves == 3, "display: and the second");

    pressDisplay(SettingsProvider::kOpenDisplays);
    monitors.pop_back(); // B unplugged
    monitors.push_back({ "C", "Projector" });
    picker.refreshDisplays();
    expect(placed.empty() && std::get<Button>(sh.published.back().content.expandedCenter[1].items[1]).glyph == Glyph::Checked &&
           savedDisplay == "B",
           "display: the picked monitor unplugged: main display, but the pick is kept");
    monitors.pop_back();
    picker.refreshDisplays();
    expect(cardText(sh.published.back(), 0) == "Copy logs", "display: down to one monitor, the page closes");
    picker.stop();
    setTokens(saved);
}

} // namespace cup::test
