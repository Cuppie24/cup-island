// Home's system widgets (decision 009): which Host calls each theme and
// Bluetooth reading produces, and what the tiles say. Settings: settings_tests.cpp.

#include "expect.h"
#include "panel_host.h"
#include "providers/system/system_panels.h"

namespace cup::test {

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
}

} // namespace cup::test
