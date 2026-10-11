// Home's static widgets (decision 009): which Host calls each battery and
// Bluetooth reading produces, and what the tiles say.

#include "expect.h"
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
    Ms now() const override { return Ms{ 0 }; }
    void publishPanel(Panel p) override { published.push_back(std::move(p)); }
    void removePanel(const std::string& id) override { removed.push_back(id); }
    void wakeAt(std::optional<Ms>) override {}
};

std::string textAt(const Panel& p, std::size_t row, std::size_t item)
{
    if (row >= p.content.tile.size() || item >= p.content.tile[row].items.size()) return "?";
    auto* text = std::get_if<Text>(&p.content.tile[row].items[item]);
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
    // Battery: a 1-column widget, published on change only, no card.
    PanelHost host;
    BatteryPresenter battery(host);
    battery.show(std::nullopt);
    expect(host.published.empty() && host.removed.empty(), "no battery: nothing published, nothing to remove");
    battery.show(BatteryReading{ 82, false });
    expect(host.published.size() == 1 && host.published[0].id == BatteryPresenter::kPanelId, "battery publishes its panel");
    const Panel& b = host.published[0];
    expect(!b.wide && !hasCard(b.content), "a 1-column static widget");
    expect(glyphAt(b, 0) == Glyph::Battery && textAt(b, 1, 0) == "82%", "battery icon over the percentage");
    battery.show(BatteryReading{ 82, false });
    expect(host.published.size() == 1, "same reading: no republish");
    battery.show(BatteryReading{ 83, true });
    expect(host.published.size() == 2 && glyphAt(host.published[1], 0) == Glyph::Charging, "plugged in: the bolt");
    battery.show(BatteryReading{ 140, true });
    expect(textAt(host.published.back(), 1, 0) == "100%", "clamped to 100%");
    battery.show(std::nullopt);
    expect(host.removed.size() == 1 && host.removed[0] == BatteryPresenter::kPanelId, "battery gone: panel removed");

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
}

} // namespace cup::test
