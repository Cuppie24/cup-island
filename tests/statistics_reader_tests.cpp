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
    CpuLoads loads = cpuLoads({ { "0,1", 20 }, { "0,_Total", 15 }, { "_Total", 17.5 }, { "1,0", 40 }, { "0,0", 10 }, { "0,10", 30 }, { "0,2", 25 } });
    expect(loads.total == 17.5f, "the total is the _Total instance");
    expect(loads.cores == std::vector<float>{ 10, 20, 25, 30, 40 }, "cores by group, then number (10 after 2), a group's total left out");
    CpuLoads old = cpuLoads({ { "1", 5 }, { "0", 7 }, { "_Total", 6 } });
    expect(old.cores == std::vector<float>{ 7, 5 } && old.total == 6.0f, "the old Processor object: plain numbers");
    expect(cpuLoads({}) == CpuLoads{} && !cpuLoads({ { "0,0", 3 } }).total, "nothing in, nothing out; no _Total, no total");

    expect(busyPercent(75, 80, 20) == 25.0f, "busy: all but idle, kernel time includes idle");
    expect(!busyPercent(0, 0, 0) && !busyPercent(10, 4, 4), "no time passed, or more idle than time: no reading");
}

void gpuParsingTests()
{
    auto engine = parseGpuEngine("pid_1234_luid_0x00000001_0x0000C5B5_phys_0_eng_3_engtype_VideoDecode");
    expect(engine && engine->pid == 1234 && engine->adapter == 0x10000C5B5ull && engine->engine == 3 && engine->type == "VideoDecode", "an engine's instance, taken apart");
    auto compute = parseGpuEngine("pid_8_luid_0x00000000_0x0000D3EC_phys_0_eng_12_engtype_Compute_0");
    expect(compute && compute->engine == 12 && compute->type == "Compute_0", "a type keeps its own underscore");
    expect(!parseGpuEngine("") && !parseGpuEngine("_Total") && !parseGpuEngine("pid_x_luid_0x0_0x1_phys_0_eng_0_engtype_3D") &&
           !parseGpuEngine("pid_4_luid_0x0_0x1_phys_0_eng_0_engtype_") && !parseGpuEngine("pid_4_phys_0_eng_0_engtype_3D"), "anything else isn't one");
    expect(parseAdapterLuid("luid_0x00000000_0x0000D3EC_phys_0") == 0xD3ECull && !parseAdapterLuid("phys_0") && !parseAdapterLuid("luid_0x1") &&
           !parseAdapterLuid("luid_zz_0x1"), "an adapter's luid, from its memory instance too");
    expect(engineName("3D") == "3D" && engineName("VideoDecode") == "Video decode" && engineName("VideoProcessing") == "Video processing" &&
           engineName("Compute_0") == "Compute 0" && engineName("Copy") == "Copy" && engineName("GDI Render") == "GDI Render", "engine types as words");
}

const NamedValue* named(const std::vector<NamedValue>& values, const std::string& name)
{
    for (const NamedValue& v : values)
        if (v.name == name) return &v;
    return nullptr;
}

void gpuLoadTests()
{
    const std::string a = "_luid_0x00000000_0x0000000A_phys_0_", b = "_luid_0x00000000_0x0000000B_phys_0_";
    std::vector<GpuLoad> loads = gpuLoads({
        { "pid_10" + a + "eng_0_engtype_3D", 30 },
        { "pid_20" + a + "eng_0_engtype_3D", 12 },
        { "pid_10" + a + "eng_1_engtype_VideoDecode", 5 },
        { "pid_20" + a + "eng_2_engtype_3D", 8 }, // a second 3D engine: the busier one speaks for the type
        { "pid_30" + b + "eng_0_engtype_3D", 3 },
        { "pid_30" + b + "eng_0_engtype_3D", kNan },
        { "nonsense", 99 },
    });
    expect(loads.size() == 2 && loads[0].adapter == 0xA && loads[1].adapter == 0xB, "an entry per adapter");
    expect(loads[0].load == 42 && loads[1].load == 3, "an adapter's load: its busiest engine, that engine's processes together");
    auto threeD = named(loads[0].engines, "3D");
    auto decode = named(loads[0].engines, "Video decode");
    expect(loads[0].engines.size() == 2 && threeD && threeD->value == 42 && decode && decode->value == 5, "engines by type, never added across engines");
    expect(loads[0].processes == std::vector<PidValue>{ { 10, 30 }, { 20, 12 } }, "a process: its busiest engine");

    std::vector<GpuAdapter> adapters = { { 0xA, "Radeon 780M", 512, false }, { 0xB, "GeForce RTX 4060", 8000, false }, { 0xC, "Microsoft Basic Render Driver", 0, true } };
    std::vector<CounterValue> memory = { { "luid_0x00000000_0x0000000A_phys_0", 100 }, { "luid_0x00000000_0x0000000B_phys_0", 2000 } };
    std::vector<CounterValue> ids = { { "chrome", 10 }, { "chrome#1", 20 }, { "game", 30 }, { "_Total", 0 }, { "Idle", 0 } };
    StatsReading busy;
    fillGpu(busy, adapters, true, loads, memory, ids);
    expect(busy.gpuName == "Radeon 780M" && busy.gpuLoad == 42.0f && busy.gpuMemoryUsed == 100u && busy.gpuMemoryTotal == 512u, "the busiest adapter is the reading's");
    expect(busy.gpuProcesses == std::vector<NamedValue>{ { "chrome", 42 } } && busy.gpuEngines.size() == 2, "its processes by name, together; its engines");

    StatsReading idle;
    fillGpu(idle, adapters, true, {}, memory, {});
    expect(idle.gpuName == "GeForce RTX 4060" && idle.gpuLoad == 0.0f && idle.gpuMemoryTotal == 8000u, "all idle: the one with the most memory, at 0 %");
    StatsReading unread;
    fillGpu(unread, adapters, false, {}, {}, {});
    expect(!unread.gpuLoad && unread.gpuName == "GeForce RTX 4060" && !unread.gpuMemoryUsed && !unread.gpuMemoryTotal, "counters not read: no load (no GPU tile), memory both or neither");
    StatsReading none;
    fillGpu(none, { adapters[2] }, true, loads, memory, ids);
    expect(none == StatsReading{}, "only the software renderer: no GPU");
}

void processTests()
{
    expect(processName("chrome#12") == "chrome" && processName("chrome") == "chrome" && processName("") == "", "PDH's #n taken off");
    auto cpu = byProcessName({ { "chrome", 40 }, { "chrome#1", 80 }, { "Idle", 700 }, { "_Total", 800 }, { "code", 16 }, { "odd", kNan }, { "neg", -1 } }, 8);
    expect(cpu == std::vector<NamedValue>{ { "chrome", 15 }, { "code", 2 } }, "an app's processes together, of all cores; Idle, _Total and non-values out");
    expect(byProcessName({ { "a", 1 } }, 0).empty(), "no cores: nothing rather than infinity");
    auto gpu = namedGpuProcesses({ { 10, 5 }, { 99, 50 }, { 0, 3 } }, { { "dwm", 10 }, { "_Total", 0 } });
    expect(gpu == std::vector<NamedValue>{ { "dwm", 5 } }, "a pid without a name (it ended), and pid 0, are left out");
}

void temperatureTests()
{
    auto hot = hottestZone({ { "\\_TZ.TZ00", 3182 }, { "\\_TZ.CPUZ", 3331 }, { "\\_TZ.DEAD", 0 } }, 10);
    expect(hot && std::fabs(*hot - 59.95f) < 0.01f, "the hottest zone, from tenths of a kelvin; a zone at 0 K isn't one");
    auto whole = hottestZone({ { "z", 300 } }, 1);
    expect(whole && std::fabs(*whole - 26.85f) < 0.01f && !hottestZone({}, 10) && !hottestZone({ { "z", kNan } }, 10), "whole kelvins; no zones, no reading");
}

AdapterSample adapter(std::uint64_t id, std::uint64_t received, std::uint64_t sent, bool up = true, bool wired = false)
{
    return { .id = id, .name = wired ? "Ethernet" : "Wi-Fi", .received = received, .sent = sent, .linkSpeed = 1000, .wired = wired, .up = up };
}

void trafficTests()
{
    std::vector<AdapterSample> before = { adapter(1, 1000, 100), adapter(2, 5000, 500) };
    expect(trafficSince(before, { adapter(1, 1600, 150), adapter(2, 5000, 500) }) == Traffic{ 600, 50 }, "what went through since: every adapter's difference");
    expect(trafficSince(before, { adapter(1, 40, 7), adapter(2, 5000, 500) }) == Traffic{ 40, 7 }, "a counter lower than before began again: all of it is new");
    expect(trafficSince(before, { adapter(1, 1000, 100), adapter(3, 9000, 900) }) == Traffic{}, "a new adapter says nothing about this span; a gone one neither");
    expect(perSecond(500, 250) == 2000.0f && !perSecond(500, 0) && !perSecond(500, -5), "bytes per second; none over no time");

    std::vector<AdapterSample> now = { adapter(1, 1600, 150), adapter(2, 5900, 500, true, true), adapter(4, 99999, 0, false) };
    expect(adapterInUse(now, before, std::nullopt)->id == 2, "in use: the connected one most went through");
    std::vector<AdapterSample> quiet = { adapter(1, 1000, 100), adapter(2, 5000, 500, true, true) };
    expect(adapterInUse(quiet, before, std::nullopt)->id == 2 && adapterInUse(quiet, before, 1)->id == 1 && adapterInUse(quiet, before, 2)->id == 2,
           "nothing moving: the one picked last time, else the cable");
    expect(!adapterInUse({ adapter(1, 5, 5, false) }, {}, std::nullopt) && !adapterInUse({}, {}, 1), "none connected: none");
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
    expect(devices[0] == BluetoothDevice{ "WH-1000XM4", 70.0f, DeviceKind::Headphones, {} }, "named by its device node, the level from its service's, the kind from its class");
    expect(devices[1] == BluetoothDevice{ "MX Anywhere 3", 55.0f, DeviceKind::Other, {} }, "Bluetooth LE: no class, and a name that doesn't say");
    expect(devices[2] == BluetoothDevice{ "Pixel Buds", 90.0f, DeviceKind::Earbuds, {} }, "no device node: the service's name without its suffix");
    expect(!devices[3].battery && devices[3].name == "Loose" && devices[4].name == "Loose", "one without a level stays, for the page to drop");
    expect(bluetoothDevices({}).empty(), "no nodes, no devices");
}

} // namespace

void runStatisticsReaderTests()
{
    cpuTests();
    gpuParsingTests();
    gpuLoadTests();
    processTests();
    temperatureTests();
    trafficTests();
    bluetoothTests();
}

} // namespace cup::test
