// Bluetooth (decision 164): which devices are connected, from Windows'
// device records and the radio; the widget by device count, off at the same
// height, its switch; the card; a battery's level as its text's colour.

#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/system/bluetooth_panel.h"

namespace cup::test {
namespace {

BluetoothNode device(const std::string& container, const std::string& name, bool connected, std::optional<int> battery = std::nullopt)
{
    return { .id = container + "/dev", .container = container, .name = name, .connected = connected, .battery = battery };
}

Tone toneAt(const Panel& p, std::size_t row, bool trailing = false, std::size_t item = 0)
{
    const Row& r = p.content.tile[row];
    const auto& items = trailing ? r.trailing : r.items;
    auto* text = item < items.size() ? std::get_if<Text>(&items[item]) : nullptr;
    return text ? text->tone : Tone::Plain;
}

} // namespace

void runBluetoothTests()
{
    // The rule: any of a device's nodes connected, and the radio not off.
    BluetoothState s;
    s.radio(BluetoothRadio::On);
    s.node(device("hp", "WH-1000XM4", false));
    expect(s.connected().empty(), "paired, not connected: not shown");
    s.node({ .id = "hp/a2dp", .container = "hp", .connected = true, .battery = 70 });
    auto shown = s.connected();
    expect(shown.size() == 1 && shown[0].name == "WH-1000XM4" && shown[0].battery == 70,
           "a service node connected: the device is, named by its own node, with the battery a node keeps");
    s.node({ .id = "hp/hfp", .container = "hp", .battery = 75 });
    expect(s.connected()[0].battery == 75, "the best level any of its nodes reports");
    s.node(device("mouse", "Mouse", true));
    s.node(device("kb", "Keyboard", true, 15));
    shown = s.connected();
    expect(shown.size() == 3 && shown[0].id == "hp" && shown[1].id == "mouse" && shown[2].id == "kb", "in the order they connected");
    s.node({ .id = "hp/a2dp", .connected = false });
    s.node({ .id = "hp/a2dp", .connected = true });
    shown = s.connected();
    expect(shown.size() == 3 && shown[2].id == "hp", "an update carries only what changed; reconnected, it goes last");
    s.radio(BluetoothRadio::Off);
    expect(s.connected().empty(), "the radio off: nothing connected, whatever the records still say");
    s.radio(BluetoothRadio::On);
    s.nodeRemoved("mouse/dev");
    expect(s.connected().size() == 2, "a node gone (unpaired): its device with it");

    // Tones (decision 164): above batteryGoodLevel good, above batteryLowLevel fair, else low.
    const Tokens& t = tokens();
    int good = static_cast<int>(t.batteryGoodLevel), low = static_cast<int>(t.batteryLowLevel);
    expect(batteryTone(good + 1) == Tone::Good && batteryTone(good) == Tone::Fair && batteryTone(low + 1) == Tone::Fair &&
               batteryTone(low) == Tone::Low,
           "a battery's tone by its level");

    // The widget, by count; always two lines, so it never changes height.
    BluetoothDevice hp{ "hp", "WH-1000XM4", 80 }, mouse{ "mouse", "Mouse", std::nullopt }, kb{ "kb", "Keyboard", 15 };
    auto one = bluetoothPanel({ BluetoothRadio::On, { hp } });
    expect(one && one->id == BluetoothPresenter::kPanelId && one->wide && hasCard(one->content) && one->content.tile.size() == 2,
           "a wide widget with a card, two lines");
    expect(textAt(*one, 0, 0) == "WH-1000XM4" && textAt(*one, 1, 0) == "80%" && toneAt(*one, 1) == Tone::Good,
           "one device: its name over its battery, the number in its tone");
    auto noBattery = bluetoothPanel({ BluetoothRadio::On, { mouse } });
    expect(textAt(*noBattery, 1, 0) == "Connected" && toneAt(*noBattery, 1) == Tone::Plain, "no battery: just connected, plain");
    auto two = bluetoothPanel({ BluetoothRadio::On, { hp, kb } });
    expect(two->content.tile.size() == 2 && textAt(*two, 0, 0) == "WH-1000XM4" && textAt(*two, 1, 0) == "Keyboard" &&
               toneAt(*two, 1, true) == Tone::Low,
           "two: a line each, the battery at its end");
    auto three = bluetoothPanel({ BluetoothRadio::On, { hp, mouse, kb } });
    expect(three->content.tile.size() == 2 && textAt(*three, 0, 0) == "3 devices" && textAt(*three, 1, 0) == "80%" &&
               textAt(*three, 1, 1) == "15%",
           "three: how many, over the batteries side by side");
    auto none = bluetoothPanel({ BluetoothRadio::On, {} });
    expect(textAt(*none, 1, 0) == "No devices connected", "on, nothing connected: says so");

    // Off: says so, never a dimmed list; the same two lines; the switch stays.
    auto off = bluetoothPanel({ BluetoothRadio::Off, { hp } });
    expect(off->content.tile.size() == 2 && textAt(*off, 0, 0) == "Bluetooth is off" && textAt(*off, 1, 0) == "Turn on to see your devices",
           "off: the message, in the list's two lines");
    auto* offSwitch = off->content.tileControls.primary ? std::get_if<Switch>(&*off->content.tileControls.primary) : nullptr;
    auto* onSwitch = one->content.tileControls.primary ? std::get_if<Switch>(&*one->content.tileControls.primary) : nullptr;
    expect(offSwitch && !offSwitch->on && onSwitch && onSwitch->on && onSwitch->actionId == bluetooth_action::kToggle &&
               !one->content.tileControls.before && !one->content.tileControls.after,
           "the switch, alone in the middle of the column (decision 163)");
    expect(bluetoothPanel({ BluetoothRadio::Unknown, { hp } })->content.tileControls.empty(), "the radio not read yet: no switch");
    expect(!bluetoothPanel({ BluetoothRadio::Missing, {} }), "no Bluetooth on this PC: no widget");

    // Same height on and off on Home (decision 163's widget, decision 164).
    auto homeHeight = [](const Panel& p) {
        IslandState st = reduce(IslandState{}, PanelPublished{ p }, Ms{ 0 });
        return layout(reduce(st, PillClicked{}, Ms{ 0 }), Ms{ 0 }).size.y;
    };
    expect(homeHeight(*off) == homeHeight(*three) && homeHeight(*one) == homeHeight(*off), "toggling doesn't change the widget's height");

    // The card: the switch at the top, every device with its level.
    const Slot& card = three->content.expandedCenter;
    expect(card.size() == 4 && std::holds_alternative<Switch>(card[0].trailing[0]), "the card: the switch, then a row per device");
    expect(std::holds_alternative<Progress>(card[1].items[2]) && std::get<Text>(card[1].trailing[0]).tone == Tone::Good,
           "a device's level: a gauge, and its number in its tone");
    expect(std::get<Text>(card[2].trailing[0]).value == "Connected", "no battery: connected");
    expect(off->content.expandedCenter.size() == 2, "off: the card says so, no list");

    // Published on change only; removed with no Bluetooth.
    PanelHost host;
    BluetoothPresenter presenter(host);
    presenter.show({ BluetoothRadio::On, { hp } });
    presenter.show({ BluetoothRadio::On, { hp } });
    expect(host.published.size() == 1, "the same: no republish");
    presenter.show({ BluetoothRadio::Off, { hp } });
    expect(host.published.size() == 2, "a change: republished");
    presenter.show({ BluetoothRadio::Missing, {} });
    expect(host.removed == std::vector<std::string>{ BluetoothPresenter::kPanelId }, "gone: removed");
}

} // namespace cup::test
