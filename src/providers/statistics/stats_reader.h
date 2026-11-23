#pragma once
// The statistics reader (decisions 173, 174): takes a StatsReading off the
// UI thread (decision 144: PDH, the device tree and the adapter table are
// calls into Windows' services, and none has a timeout).
//
// It reads what a request asks for and nothing else: each source (the
// CPU's counter, the GPU's, the thermal zones', the adapters' table) is
// opened when first asked for and closed once it hasn't been for
// `idleClose`. A pill showing only memory never opens a counter.
//
// Threading: request() and stop() are the UI thread's. Everything else
// runs on the reader's own thread, which sleeps on a condition variable
// until a reading is asked for: nothing is polled, and when the provider
// stops asking nothing runs at all (TR §8). The reading is handed to
// `done` on the reader's thread; the provider posts it to the UI thread.

#include "providers/statistics/stats_reading.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace cup {

// The reader's timings, from the tokens of the same names (statsFirstSpan…).
// Handed over with each request: tokens() is the UI thread's, and a reload
// rewrites it (decision 144).
struct StatsReaderSettings {
    // A rate (a load, a speed) needs two samples: with none recent enough of its own, the reader takes one, waits this long and reads.
    std::chrono::milliseconds firstSpan{ 250 };
    // A sample older than this isn't "since the last reading" any more.
    std::chrono::milliseconds staleAfter{ 10'000 };
    // A source not asked for this long is closed.
    std::chrono::milliseconds idleClose{ 30'000 };
};

class StatsReader {
public:
    using Done = std::function<void(StatsReading)>; // on the reader's thread

    explicit StatsReader(Done done);
    ~StatsReader(); // stop()
    StatsReader(const StatsReader&) = delete;
    StatsReader& operator=(const StatsReader&) = delete;

    // A reading of what `request` asks; returns at once. One asked while
    // another is under way is read right after it, together with anything
    // else asked meanwhile: a Bluetooth listing due once a minute is never dropped.
    void request(const StatsRequest& request, const StatsReaderSettings& settings);
    // Ends the thread and waits for it: at most a reading's length, and no
    // `done` after it returns.
    void stop();

private:
    struct Sources; // what's open and the last samples: the reader thread's only

    void run();
    std::optional<StatsReading> read(Sources& sources, const StatsRequest& request, const StatsReaderSettings& settings); // none: stopped meanwhile
    bool waitFor(std::chrono::milliseconds span); // false: stopped meanwhile

    Done done_;
    std::mutex mutex_;
    std::condition_variable wake_;
    StatsRequest wanted_;          // asked and not yet taken; nothing in it: nothing asked
    StatsReaderSettings settings_; // ...and the settings the last request came with
    bool stopped_ = false;
    std::thread thread_;
};

} // namespace cup
