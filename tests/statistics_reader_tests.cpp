// Statistics, the reader's rules (decision 173): what Windows' counters,
// adapter table and device tree say, made into a reading's values. The
// looks themselves are Windows-only; everything decided about them is here.

#include "expect.h"
#include "providers/statistics/stats_bluetooth.h"
#include "providers/statistics/stats_counters.h"
#include "providers/statistics/stats_network.h"

#include <cmath>
#include <limits>

namespace cup::test {
namespace {

const double kNan = std::numeric_limits<double>::quiet_NaN();

void cpuTests()
{
    expect(busyPercent(75, 80, 20) == 25.0f, "busy: all but idle, kernel time includes idle");
    expect(!busyPercent(0, 0, 0) && !busyPercent(10, 4, 4), "no time passed, or more idle than time: no reading");
}

void gpuTests()
{
    auto engine = parseGpuEngine("pid_1234_luid_0x00000001_0x0000C5B5_phys_0_eng_3_engtype_VideoDecode");
    expect(engine && engine->adapter == 0x10000C5B5ull && engine->engine == 3, "an engine's instance: its adapter's luid and its number");
    auto compute = parseGpuEngine("pid_8_luid_0x00000000_0x0000D3EC_phys_0_eng_12_engtype_Compute_0");
    expect(compute && compute->adapter == 0xD3ECull && compute->engine == 12, "whatever its type is called");
    expect(!parseGpuEngine("") && !parseGpuEngine("_Total") && !parseGpuEngine("pid_4_luid_0x0_0x1_phys_0_eng_x_engtype_3D") &&
           !parseGpuEngine("pid_4_phys_0_eng_0_engtype_3D") && !parseGpuEngine("pid_4_luid_zz_0x1_phys_0_eng_0_engtype_3D") &&
           !parseGpuEngine("luid_0x00000000_0x0000D3EC_phys_0"), "anything else isn't one");

    const std::string a = "_luid_0x00000000_0x0000000A_phys_0_", b = "_luid_0x00000000_0x0000000B_phys_0_";
    std::vector<GpuLoad> loads = gpuLoads({
        { "pid_10" + a + "eng_0_engtype_3D", 30 },
        { "pid_20" + a + "eng_0_engtype_3D", 12 },
        { "pid_10" + a + "eng_1_engtype_VideoDecode", 5 },
        { "pid_20" + a + "eng_2_engtype_3D", 8 },
        { "pid_30" + b + "eng_0_engtype_3D", 3 },
        { "pid_30" + b + "eng_0_engtype_3D", kNan },
        { "nonsense", 99 },
    });
    expect(loads == std::vector<GpuLoad>{ { 0xA, 42 }, { 0xB, 3 } }, "an adapter's load: its busiest engine, that engine's processes together, never added across engines");

    std::vector<GpuAdapter> adapters = { { 0xA, 512, false }, { 0xB, 8000, false }, { 0xC, 0, true } };
    const GpuAdapter* busy = chooseGpu(adapters, loads);
    expect(busy && busy->luid == 0xA && gpuLoadOf(*busy, loads) == 42.0f, "the busiest hardware adapter is the reading's");
    const GpuAdapter* idle = chooseGpu(adapters, {});
    expect(idle && idle->luid == 0xB && gpuLoadOf(*idle, {}) == 0.0f, "all idle, or not measured (its temperature alone asked): the one with the most memory, at 0 %");
    expect(!chooseGpu({ adapters[2] }, { { 0xC, 80 } }) && !chooseGpu({}, loads), "only the software renderer, or none: no GPU");
}

void temperatureTests()
{
    auto hot = hottestZone({ { "\\_TZ.TZ00", 3182 }, { "\\_TZ.CPUZ", 3331 }, { "\\_TZ.DEAD", 0 } }, 10);
    expect(hot && std::fabs(*hot - 59.95f) < 0.01f, "the hottest zone, from tenths of a kelvin; a zone at 0 K isn't one");
    auto whole = hottestZone({ { "z", 300 } }, 1);
    expect(whole && std::fabs(*whole - 26.85f) < 0.01f && !hottestZone({}, 10) && !hottestZone({ { "z", kNan } }, 10), "whole kelvins; no zones, no reading");
}

void trafficTests()
{
    std::vector<AdapterSample> before = { { 1, 1000, 100 }, { 2, 5000, 500 } };
    expect(trafficSince(before, { { 1, 1600, 150 }, { 2, 5000, 500 } }) == Traffic{ 600, 50 }, "what went through since: every adapter's difference");
    expect(trafficSince(before, { { 1, 40, 7 }, { 2, 5000, 500 } }) == Traffic{ 40, 7 }, "a counter lower than before began again: all of it is new");
    expect(trafficSince(before, { { 1, 1000, 100 }, { 3, 9000, 900 } }) == Traffic{}, "a new adapter says nothing about this span; a gone one neither");
    expect(perSecond(500, 250) == 2000.0f && !perSecond(500, 0) && !perSecond(500, -5), "bytes per second; none over no time");
}

void bluetoothTests()
{
    expect(kindOfClass(0x240404) == DeviceKind::Headphones && kindOfClass(0x240418) == DeviceKind::Headphones, "audio: a headset, headphones");
    expect(kindOfClass(0x002580) == DeviceKind::Mouse && kindOfClass(0x002540) == DeviceKind::Keyboard && kindOfClass(0x002508) == DeviceKind::Controller &&
           kindOfClass(0x00251C) == DeviceKind::Pen, "peripherals: a mouse, a keyboard, a gamepad, a pen");
    expect(kindOfClass(0x5A020C) == DeviceKind::Other && kindOfClass(0x240414) == DeviceKind::Other, "a phone, a loudspeaker: other");
    expect(kindOfName("Galaxy Buds2 Pro", DeviceKind::Headphones) == DeviceKind::Earbuds && kindOfName("AirPods Pro", DeviceKind::Other) == DeviceKind::Earbuds,
           "earbuds call themselves headsets: the name says");
    expect(kindOfName("MX Master 3S", DeviceKind::Mouse) == DeviceKind::Mouse && kindOfName("Bluetooth Mouse M240", DeviceKind::Other) == DeviceKind::Mouse &&
           kindOfName("Surface Pen", DeviceKind::Other) == DeviceKind::Pen && kindOfName("OpenRun Pro", DeviceKind::Other) == DeviceKind::Other &&
           kindOfName("Xbox Wireless Controller", DeviceKind::Other) == DeviceKind::Controller, "a known kind stays; else a guess from the name's words");

    std::vector<BluetoothDevice> devices = bluetoothDevices({
        { "c1", "BTHENUM\\{0000111E-0000-1000-8000-00805F9B34FB}_VID&0001\\7&1&0&AC80_C0", "WH-1000XM4 Hands-Free AG", 70.0f, std::nullopt, std::nullopt },
        { "c1", "BTHENUM\\DEV_AC800A0B0C0D\\7&1&0&BLUETOOTHDEVICE_AC80", "WH-1000XM4", std::nullopt, true, 0x240404 },
        { "c1", "BTHENUM\\{0000110B-0000-1000-8000-00805F9B34FB}_VID&0001\\7&1&0&AC80_C0", "WH-1000XM4 Stereo", std::nullopt, std::nullopt, std::nullopt },
        { "c2", "BTHENUM\\DEV_111111111111\\7&1", "Old Speaker", std::nullopt, false, 0x240414 },
        { "c2", "BTHENUM\\{0000111E-0000-1000-8000-00805F9B34FB}\\7&2", "Old Speaker Hands-Free AG", 40.0f, std::nullopt, std::nullopt },
        { "c3", "BTHLE\\DEV_222222222222\\8&1", "MX Anywhere 3", 55.0f, std::nullopt, std::nullopt },
        { "c4", "BTHENUM\\{0000111E-0000-1000-8000-00805F9B34FB}\\7&3", "Pixel Buds Hands-Free AG", 90.0f, std::nullopt, std::nullopt },
        { "", "BTHLEDEVICE\\{0000180F}\\9&1", "Loose", std::nullopt, std::nullopt, std::nullopt },
        { "", "BTHLEDEVICE\\{0000180A}\\9&2", "Loose", std::nullopt, std::nullopt, std::nullopt },
    });
    expect(devices.size() == 5, "a device per container; a disconnected one is left out, nodes with no container stay apart");
    expect(devices[0] == BluetoothDevice{ "WH-1000XM4", 70.0f, DeviceKind::Headphones }, "named by its device node, the level from its service's, the kind from its class");
    expect(devices[1] == BluetoothDevice{ "MX Anywhere 3", 55.0f, DeviceKind::Other }, "Bluetooth LE: no class, and a name that doesn't say");
    expect(devices[2] == BluetoothDevice{ "Pixel Buds", 90.0f, DeviceKind::Earbuds }, "no device node: the service's name without its suffix");
    expect(!devices[3].battery && devices[3].name == "Loose" && devices[4].name == "Loose", "one without a level stays, for the page to drop");
    expect(bluetoothDevices({}).empty(), "no nodes, no devices");
}

} // namespace

void runStatisticsReaderTests()
{
    cpuTests();
    gpuTests();
    temperatureTests();
    trafficTests();
    bluetoothTests();
}

} // namespace cup::test
