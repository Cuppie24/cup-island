#include "providers/statistics/stats_reader.h"

#include "platform/log.h"
#include "providers/statistics/stats_sources.h"

#include <exception>
#include <utility>
#include <vector>

namespace cup {
namespace {

using Clock = std::chrono::steady_clock;

// Decision 158's rule for a thread of our own: nothing a look throws gets
// out (an uncaught exception on a std::thread ends the app). The look is
// logged and its values stay out of the reading.
void guarded(const char* what, const std::function<void()>& look)
{
    try {
        look();
    } catch (const std::exception& e) {
        platform::logLine("statistics: reading %s failed (%s)", what, e.what());
    } catch (...) {
        platform::logLine("statistics: reading %s failed", what);
    }
}

std::optional<float> asFloat(std::optional<double> value)
{
    return value ? std::optional<float>(static_cast<float>(*value)) : std::nullopt;
}

bool wantsProcesses(StatsPage page) // the pages with a list of them (decision 172)
{
    return page == StatsPage::Cpu || page == StatsPage::Gpu || page == StatsPage::Memory;
}

} // namespace

// The reader thread's state: the open counters, and the last sample of
// everything a rate is worked out from.
struct StatsReader::Sources {
    using Counter = stats::CounterSet::Counter;

    std::unique_ptr<stats::CounterSet> counters; // null: closed
    Counter cpu = nullptr, diskRead = nullptr, diskWrite = nullptr, zones = nullptr, preciseZones = nullptr;
    Counter standby[3] = {};
    Counter gpuEngines = nullptr, gpuMemory = nullptr;
    // Processes are the costly counters: open only on the pages that list them.
    std::unique_ptr<stats::CounterSet> processes;
    Counter processCpu = nullptr, processMemory = nullptr, processIds = nullptr;

    Clock::time_point sampled{};
    std::optional<stats::CpuTimes> cpuTimes;
    std::optional<std::vector<AdapterSample>> adapters; // none: not looked at yet
    std::optional<std::uint64_t> adapterInUse;

    std::string cpuName; // what doesn't change while the card is open: read at open()
    std::size_t cores = 1;
    std::vector<GpuAdapter> gpus;
    std::vector<BluetoothDevice> bluetooth; // the last listing, shown until the next
    std::size_t untilBluetooth = 0;
    StatsPage lastPage = StatsPage::Overview;

    void open()
    {
        counters = std::make_unique<stats::CounterSet>();
        // Utility is what Task Manager shows (Windows 8+: it follows the
        // clock speed); Time where it's missing, the old object last.
        cpu = counters->add(L"\\Processor Information(*)\\% Processor Utility");
        bool utility = cpu != nullptr;
        if (!cpu) cpu = counters->add(L"\\Processor Information(*)\\% Processor Time");
        if (!cpu) cpu = counters->add(L"\\Processor(*)\\% Processor Time");
        diskRead = counters->add(L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec");
        diskWrite = counters->add(L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec");
        standby[0] = counters->add(L"\\Memory\\Standby Cache Normal Priority Bytes");
        standby[1] = counters->add(L"\\Memory\\Standby Cache Reserve Bytes");
        standby[2] = counters->add(L"\\Memory\\Standby Cache Core Bytes");
        preciseZones = counters->add(L"\\Thermal Zone Information(*)\\High Precision Temperature"); // tenths of a kelvin
        zones = counters->add(L"\\Thermal Zone Information(*)\\Temperature");                      // kelvins
        gpuEngines = counters->add(L"\\GPU Engine(*)\\Utilization Percentage");                    // Windows 10 1709+
        gpuMemory = counters->add(L"\\GPU Adapter Memory(*)\\Dedicated Usage");
        cpuName = stats::cpuName();
        cores = stats::coreCount();
        gpus = stats::gpuAdapters();
        untilBluetooth = 0;
        platform::logLine("statistics: counters opened (cpu %s, disk %d, standby %d, thermal zones %d, gpu engines %d, gpu memory %d; %u graphics adapter(s))",
                          utility ? "utility" : cpu ? "time" : "none", diskRead != nullptr, standby[0] != nullptr,
                          preciseZones != nullptr || zones != nullptr, gpuEngines != nullptr, gpuMemory != nullptr, static_cast<unsigned>(gpus.size()));
    }

    void openProcesses()
    {
        processes = std::make_unique<stats::CounterSet>();
        processCpu = processes->add(L"\\Process(*)\\% Processor Time");
        processMemory = processes->add(L"\\Process(*)\\Working Set - Private"); // Task Manager's "Memory" column
        processIds = processes->add(L"\\Process(*)\\ID Process");
    }

    // The samples the next reading's rates start from.
    void sample()
    {
        counters->collect();
        cpuTimes = stats::cpuTimes();
        adapters = stats::networkAdapters();
        sampled = Clock::now();
    }

    void close()
    {
        processes.reset();
        counters.reset();
        cpuTimes.reset();
        adapters.reset();
        bluetooth.clear();
    }
};

StatsReader::StatsReader(Done done) : done_(std::move(done))
{
    thread_ = std::thread([this] { run(); });
}

StatsReader::~StatsReader()
{
    stop();
}

void StatsReader::request(StatsPage page, const StatsReaderSettings& settings)
{
    {
        std::lock_guard lock(mutex_);
        if (stopped_ || reading_) return;
        wanted_ = page;
        settings_ = settings;
    }
    wake_.notify_one();
}

void StatsReader::stop()
{
    {
        std::lock_guard lock(mutex_);
        stopped_ = true;
    }
    wake_.notify_one();
    if (thread_.joinable()) thread_.join();
}

bool StatsReader::waitFor(std::chrono::milliseconds span)
{
    std::unique_lock lock(mutex_);
    return !wake_.wait_for(lock, span, [this] { return stopped_; });
}

void StatsReader::run()
{
    Sources sources;
    for (;;) {
        StatsPage page = StatsPage::Overview;
        StatsReaderSettings settings;
        {
            std::unique_lock lock(mutex_);
            reading_ = false;
            auto called = [this] { return stopped_ || wanted_.has_value(); };
            if (!sources.counters) wake_.wait(lock, called); // closed: asleep until asked
            else if (!wake_.wait_for(lock, settings_.idleClose, called)) {
                lock.unlock();
                guarded("closing", [&] { sources.close(); }); // the card was closed: hold nothing
                continue;
            }
            if (stopped_) return;
            page = *wanted_;
            settings = settings_;
            wanted_.reset();
            reading_ = true;
        }
        std::optional<StatsReading> reading;
        guarded("the machine", [&] { reading = read(sources, page, settings); });
        {
            std::lock_guard lock(mutex_);
            if (stopped_) return;
        }
        if (reading) guarded("(handing it over)", [&] { done_(std::move(*reading)); });
    }
}

StatsReading StatsReader::read(Sources& s, StatsPage page, const StatsReaderSettings& settings)
{
    // Rates are "since the last sample": one from before the card was closed is no use.
    if (s.counters && Clock::now() - s.sampled > settings.staleAfter) s.close();
    bool fresh = !s.counters;
    if (fresh) {
        s.open();
        s.sample();
    }
    bool freshProcesses = wantsProcesses(page) && !s.processes;
    if (freshProcesses) {
        s.openProcesses();
        s.processes->collect();
    }
    if (!wantsProcesses(page)) s.processes.reset();
    if ((fresh || freshProcesses) && !waitFor(settings.firstSpan)) return {}; // stopped: run() drops it

    StatsReading r;
    std::optional<stats::CpuTimes> timesBefore = s.cpuTimes;
    std::vector<AdapterSample> adaptersBefore = s.adapters.value_or(std::vector<AdapterSample>{});
    bool hadAdapters = s.adapters.has_value();
    double elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - s.sampled).count();
    guarded("the samples", [&] { s.sample(); });
    std::vector<CounterValue> processIds;
    if (s.processes) guarded("processes", [&] {
        s.processes->collect();
        processIds = s.processes->values(s.processIds);
        r.cpuProcesses = byProcessName(s.processes->values(s.processCpu), static_cast<double>(s.cores));
        r.memoryApps = byProcessName(s.processes->values(s.processMemory), 1);
    });

    guarded("the CPU", [&] {
        r.cpuName = s.cpuName;
        CpuLoads loads = cpuLoads(s.counters->values(s.cpu));
        r.coreLoads = loads.cores;
        r.cpuLoad = loads.total;
        // The counters broken (a damaged counter registry isn't rare): the total from the system's times.
        if (!r.cpuLoad && timesBefore && s.cpuTimes && s.cpuTimes->kernel >= timesBefore->kernel && s.cpuTimes->user >= timesBefore->user &&
            s.cpuTimes->idle >= timesBefore->idle)
            r.cpuLoad = busyPercent(s.cpuTimes->idle - timesBefore->idle, s.cpuTimes->kernel - timesBefore->kernel, s.cpuTimes->user - timesBefore->user);
        // The hottest ACPI thermal zone, where the firmware has any: the one temperature Windows gives without admin rights (decision 173).
        r.cpuTemperature = hottestZone(s.counters->values(s.preciseZones), 10);
        if (!r.cpuTemperature) r.cpuTemperature = hottestZone(s.counters->values(s.zones), 1);
    });
    guarded("memory", [&] {
        auto memory = stats::memory();
        if (!memory) return;
        r.memoryUsed = memory->used;
        r.memoryTotal = memory->total;
        auto normal = s.counters->value(s.standby[0]), reserve = s.counters->value(s.standby[1]), core = s.counters->value(s.standby[2]);
        if (normal && reserve && core && *normal + *reserve + *core >= 0) r.memoryCached = static_cast<std::uint64_t>(*normal + *reserve + *core);
        else r.memoryCached = memory->cache;
    });
    guarded("the disks", [&] {
        r.drives = stats::drives();
        r.diskRead = asFloat(s.counters->value(s.diskRead));
        r.diskWrite = asFloat(s.counters->value(s.diskWrite));
    });
    guarded("the network", [&] {
        if (!s.adapters || s.adapters->empty()) return;
        if (hadAdapters) {
            Traffic traffic = trafficSince(adaptersBefore, *s.adapters);
            r.networkDown = perSecond(traffic.received, elapsedMs);
            r.networkUp = perSecond(traffic.sent, elapsedMs);
        }
        if (const AdapterSample* used = adapterInUse(*s.adapters, adaptersBefore, s.adapterInUse)) {
            s.adapterInUse = used->id;
            r.networkName = used->name;
            r.wired = used->wired;
            if (used->linkSpeed > 0) r.linkSpeed = used->linkSpeed;
        }
    });
    guarded("the GPU", [&] {
        bool read = false;
        std::vector<CounterValue> engines = s.counters->values(s.gpuEngines, &read);
        fillGpu(r, s.gpus, read, gpuLoads(engines), s.counters->values(s.gpuMemory), processIds);
    });
    guarded("Bluetooth", [&] {
        bool opened = page == StatsPage::Bluetooth && s.lastPage != StatsPage::Bluetooth;
        if (s.untilBluetooth == 0 || opened) {
            s.bluetooth = bluetoothDevices(stats::bluetoothNodes());
            s.untilBluetooth = settings.bluetoothEvery > 0 ? settings.bluetoothEvery : 1;
        }
        --s.untilBluetooth;
        r.bluetooth = s.bluetooth;
    });
    s.lastPage = page;
    return r;
}

} // namespace cup
