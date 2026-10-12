// Home's system widgets (decision 009): which Host calls each theme and
// Bluetooth reading produces, and what the tiles say. Plus the Settings widget.

#include "expect.h"
#include "core/tokens.h"
#include "providers/system/settings_provider.h"
#include "providers/system/system_panels.h"

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

    // Settings (decision 022): a widget whose card picks the theme and copies the logs.
    Tokens saved = tokens();
    Tokens themed = saved;
    themed.themes = { { "Midnight", {} }, { "Grape", { { "islandColor", { 24, 9, 42 } } } }, { "Lime", {} } };
    setTokens(themed);
    PanelHost sh;
    std::string stored = "Lime\r\n", applied;
    bool copyWorks = true;
    int copies = 0, saves = 0;
    SettingsProvider settings({ .loadTheme = [&] { return stored; },
                                .saveTheme = [&](const std::string& name) { stored = name; ++saves; },
                                .applyTheme = [&](const std::string& name) { applied = name; },
                                .copyLogs = [&] { ++copies; return copyWorks; } });
    settings.start(sh);
    const Panel& widget = sh.published.back();
    expect(widget.id == SettingsProvider::kPanelId && widget.tapAction.empty() && hasCard(widget.content) &&
           widget.content.card == CardLayout::Centred, "settings: a widget that opens a centred card");
    expect(glyphAt(widget, 0) == Glyph::Settings && textAt(widget, 1, 0) == "Settings", "settings: the gear");
    expect(applied == "Lime" && saves == 0 && cardText(widget, 1) == "Lime", "settings: the saved theme applies at start");

    auto press = [&](const char* actionId) { settings.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    press(SettingsProvider::kNextTheme);
    expect(applied == "Midnight" && stored == "Midnight" && cardText(sh.published.back(), 1) == "Midnight", "settings: next wraps to the first, saved");
    press(SettingsProvider::kPreviousTheme);
    press(SettingsProvider::kPreviousTheme);
    expect(applied == "Grape" && stored == "Grape" && saves == 3, "settings: previous wraps to the last, then back");

    stored = "Renamed";
    settings.stop();
    settings.start(sh);
    expect(applied == "Midnight", "settings: a saved theme tokens.json no longer has falls back to the default");
    themed.themes.erase(themed.themes.begin());
    setTokens(themed);
    settings.refresh();
    expect(applied == "Grape" && cardText(sh.published.back(), 1) == "Grape", "settings: a reload that drops the theme moves to the new first");

    // Copy logs moved here from its own tile: says how the copy went, then resets.
    expect(cardText(sh.published.back(), 2) == "Copy logs" && !sh.wake, "logs: idle, nothing scheduled");
    sh.clock = Ms{ 1000 };
    press(SettingsProvider::kCopyLogs);
    expect(copies == 1 && cardText(sh.published.back(), 2) == "Logs copied", "logs: a click copies and says so");
    expect(sh.wake == Ms{ 1000 } + tokens().logsNote, "logs: the note lasts logsNoteMs");
    settings.onWake();
    expect(cardText(sh.published.back(), 2) == "Copy logs", "logs: then back to Copy logs");
    copyWorks = false;
    press(SettingsProvider::kCopyLogs);
    expect(cardText(sh.published.back(), 2) == "Couldn't copy logs", "logs: a failed copy says so");
    settings.stop();
    expect(sh.removed.back() == SettingsProvider::kPanelId && !sh.wake, "settings: stop removes the widget and the wake-up");

    themed.themes.clear();
    setTokens(themed);
    settings.start(sh);
    expect(sh.published.back().content.expandedCenter.size() == 1 && cardText(sh.published.back(), 0) == "Copy logs",
           "settings: no themes in tokens.json, no theme picker");
    settings.stop();
    setTokens(saved);
}

} // namespace cup::test
