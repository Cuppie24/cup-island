#include "providers/statistics/stats_reader.h"

#include "platform/log.h"
#include "providers/statistics/stats_sources.h"

#include <exception>
#include <utility>
#include <vector>

namespace cup {
namespace {

using Clock = std::chrono::steady_clock;
using Counter = stats::CounterSet::Counter;

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

// When a source was last asked for, and when its last sample was taken.
struct Use {
    bool open = false;
    Clock::time_point asked{};
    std::optional<Clock::time_point> sampled; // none: no sample to work a rate out from
    bool recent(Clock::time_point now, std::chrono::milliseconds staleAfter) const { return sampled && now - *sampled <= staleAfter; }
    double sinceSampleMs(Clock::time_point now) const { return std::chrono::duration<double, std::milli>(now - *sampled).count(); }
};

} // namespace

// The reader thread's state: a part per source, each open only while it's asked for.
struct StatsReader::Sources {
    // The CPU's load: one counter, and the system's times as its fallback.
    Use cpu;
    std::unique_ptr<stats::CounterSet> cpuCounters;
    Counter cpuLoad = nullptr;
    std::optional<stats::CpuTimes> cpuTimes;
    // Thermal zones (not a rate: no first sample).
    Use zones;
    std::unique_ptr<stats::CounterSet> zoneCounters;
    Counter preciseZones = nullptr, wholeZones = nullptr;
    // The GPU's engines: the costly counter, an instance per process and engine.
    Use gpu;
    std::unique_ptr<stats::CounterSet> gpuCounters;
    Counter gpuEngines = nullptr;
    // Graphics adapters, listed once per use: for the load and the temperature both.
    Use adapters;
    std::vector<GpuAdapter> gpuAdapters;
    // Network adapters' byte counters at the last look.
    Use network;
    std::vector<AdapterSample> networkAdapters;

    void sampleCpu()
    {
        if (!cpu.open) {
            cpuCounters = std::make_unique<stats::CounterSet>();
            // Utility is what Task Manager shows (Windows 8+: it follows the
            // clock speed); Time where it's missing, the old object last.
            cpuLoad = cpuCounters->add(L"\\Processor Information(_Total)\\% Processor Utility");
            bool utility = cpuLoad != nullptr;
            if (!cpuLoad) cpuLoad = cpuCounters->add(L"\\Processor Information(_Total)\\% Processor Time");
            if (!cpuLoad) cpuLoad = cpuCounters->add(L"\\Processor(_Total)\\% Processor Time");
            platform::logLine("statistics: CPU counter opened (%s)", utility ? "utility" : cpuLoad ? "time" : "none: the system's times instead");
            cpu.open = true;
        }
        cpuCounters->collect();
        cpuTimes = stats::cpuTimes();
        cpu.sampled = Clock::now();
    }

    void sampleGpu()
    {
        if (!gpu.open) {
            gpuCounters = std::make_unique<stats::CounterSet>();
            gpuEngines = gpuCounters->add(L"\\GPU Engine(*)\\Utilization Percentage"); // Windows 10 1709+
            platform::logLine("statistics: GPU counter opened (%s)", gpuEngines ? "engines" : "none");
            gpu.open = true;
        }
        gpuCounters->collect();
        gpu.sampled = Clock::now();
    }

    void sampleNetwork()
    {
        networkAdapters = stats::networkAdapters();
        network.open = true;
        network.sampled = Clock::now();
    }

    void openZones()
    {
        if (zones.open) return;
        zoneCounters = std::make_unique<stats::CounterSet>();
        preciseZones = zoneCounters->add(L"\\Thermal Zone Information(*)\\High Precision Temperature"); // tenths of a kelvin
        wholeZones = zoneCounters->add(L"\\Thermal Zone Information(*)\\Temperature");                  // kelvins
        platform::logLine("statistics: thermal zone counters opened (%s)", preciseZones || wholeZones ? "found" : "none");
        zones.open = true;
    }

    void listAdapters()
    {
        if (adapters.open) return;
        gpuAdapters = stats::gpuAdapters();
        adapters.open = true;
    }

    bool anyOpen() const { return cpu.open || zones.open || gpu.open || adapters.open || network.open; }

    // Closes what hasn't been asked for `idleClose`: the card closed, or the pill's picks changed.
    void closeUnused(Clock::time_point now, std::chrono::milliseconds idleClose)
    {
        auto unused = [&](Use& use) {
            if (!use.open || now - use.asked <= idleClose) return false;
            use = {};
            return true;
        };
        if (unused(cpu)) cpuCounters.reset(), cpuLoad = nullptr, cpuTimes.reset();
        if (unused(zones)) zoneCounters.reset(), preciseZones = wholeZones = nullptr;
        if (unused(gpu)) gpuCounters.reset(), gpuEngines = nullptr;
        if (unused(adapters)) gpuAdapters.clear();
        if (unused(network)) networkAdapters.clear();
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

void StatsReader::request(const StatsRequest& request, const StatsReaderSettings& settings)
{
    if (!request.any()) return;
    {
        std::lock_guard lock(mutex_);
        if (stopped_) return;
        // Added to what's waiting, never instead of it.
        wanted_.cpuLoad |= request.cpuLoad, wanted_.cpuTemperature |= request.cpuTemperature;
        wanted_.gpuLoad |= request.gpuLoad, wanted_.gpuTemperature |= request.gpuTemperature;
        wanted_.memory |= request.memory, wanted_.disk |= request.disk;
        wanted_.network |= request.network, wanted_.bluetooth |= request.bluetooth;
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
        StatsRequest request;
        StatsReaderSettings settings;
        {
            std::unique_lock lock(mutex_);
            auto called = [this] { return stopped_ || wanted_.any(); };
            if (!sources.anyOpen()) wake_.wait(lock, called); // nothing open: asleep until asked
            else if (!wake_.wait_for(lock, settings_.idleClose, called)) {
                settings = settings_;
                lock.unlock();
                // Nobody asked for a while: hold nothing.
                guarded("(closing)", [&] { sources.closeUnused(Clock::now(), settings.idleClose); });
                continue;
            }
            if (stopped_) return;
            request = std::exchange(wanted_, {});
            settings = settings_;
        }
        std::optional<StatsReading> reading;
        guarded("the machine", [&] { reading = read(sources, request, settings); });
        guarded("(closing)", [&] { sources.closeUnused(Clock::now(), settings.idleClose); });
        {
            std::lock_guard lock(mutex_);
            if (stopped_) return;
        }
        if (reading) guarded("(handing it over)", [&] { done_(std::move(*reading)); });
    }
}

std::optional<StatsReading> StatsReader::read(Sources& s, const StatsRequest& request, const StatsReaderSettings& settings)
{
    Clock::time_point now = Clock::now();
    // A rate is "since the last sample". A source asked for without a
    // recent one (the first time, or after a pause) gets one now, and the
    // reading waits firstSpan: only then, and only for those sources.
    bool waits = false;
    if (request.cpuLoad && !s.cpu.recent(now, settings.staleAfter)) guarded("the CPU", [&] { s.sampleCpu(), waits = true; });
    if (request.gpuLoad && !s.gpu.recent(now, settings.staleAfter)) guarded("the GPU", [&] { s.sampleGpu(), waits = true; });
    if (request.network && !s.network.recent(now, settings.staleAfter)) guarded("the network", [&] { s.sampleNetwork(), waits = true; });
    if (waits && !waitFor(settings.firstSpan)) return std::nullopt;
    now = Clock::now();

    StatsReading r;
    if (request.cpuLoad) guarded("the CPU", [&] {
        s.cpu.asked = now;
        r.cpuCores = stats::coreCount();
        if (!s.cpu.sampled) return;
        std::optional<stats::CpuTimes> before = s.cpuTimes;
        s.sampleCpu();
        r.cpuLoad = asFloat(s.cpuCounters->value(s.cpuLoad));
        // The counters broken (a damaged counter registry isn't rare): the load from the system's times.
        const std::optional<stats::CpuTimes>& after = s.cpuTimes;
        if (!r.cpuLoad && before && after && after->kernel >= before->kernel && after->user >= before->user && after->idle >= before->idle)
            r.cpuLoad = busyPercent(after->idle - before->idle, after->kernel - before->kernel, after->user - before->user);
    });
    if (request.cpuTemperature) guarded("the thermal zones", [&] {
        s.zones.asked = now;
        s.openZones();
        s.zoneCounters->collect();
        // The hottest ACPI thermal zone, where the firmware has any: the one CPU temperature Windows gives without admin rights (decision 173).
        r.cpuTemperature = hottestZone(s.zoneCounters->values(s.preciseZones), 10);
        if (!r.cpuTemperature) r.cpuTemperature = hottestZone(s.zoneCounters->values(s.wholeZones), 1);
    });
    if (request.gpuLoad || request.gpuTemperature) guarded("the GPU", [&] {
        s.adapters.asked = now;
        s.listAdapters();
        bool read = false;
        std::vector<GpuLoad> loads;
        if (request.gpuLoad) {
            s.gpu.asked = now;
            if (s.gpu.sampled) {
                s.sampleGpu();
                loads = gpuLoads(s.gpuCounters->values(s.gpuEngines, &read));
            }
        }
        // The busiest hardware adapter; its temperature alone asked, the one with the most memory.
        const GpuAdapter* chosen = chooseGpu(s.gpuAdapters, loads);
        if (!chosen) return;
        if (read) r.gpuLoad = gpuLoadOf(*chosen, loads);
        if (request.gpuTemperature) r.gpuTemperature = stats::gpuTemperature(chosen->luid);
    });
    if (request.memory) guarded("memory", [&] {
        if (auto memory = stats::memory()) r.memoryUsed = memory->used, r.memoryTotal = memory->total;
    });
    if (request.disk) guarded("the system drive", [&] {
        if (auto drive = stats::systemDrive()) r.diskUsed = drive->used, r.diskTotal = drive->total;
    });
    if (request.network) guarded("the network", [&] {
        s.network.asked = now;
        if (!s.network.sampled) return;
        std::vector<AdapterSample> before = std::move(s.networkAdapters);
        double elapsedMs = s.network.sinceSampleMs(now);
        s.sampleNetwork();
        if (before.empty() || s.networkAdapters.empty()) return; // the table wasn't read: no adapters at all isn't a PC
        Traffic traffic = trafficSince(before, s.networkAdapters);
        r.networkDown = perSecond(traffic.received, elapsedMs);
        r.networkUp = perSecond(traffic.sent, elapsedMs);
    });
    // Only when asked (decision 042): one listing, nothing kept open, and the provider keeps the last one.
    if (request.bluetooth) guarded("Bluetooth", [&] { r.bluetooth = bluetoothDevices(stats::bluetoothNodes()); });
    return r;
}

} // namespace cup
