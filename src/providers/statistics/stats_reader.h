#pragma once
// The statistics reader (decision 173): takes a StatsReading off the UI
// thread (decision 144: PDH, the device tree and the adapter table are
// calls into Windows' services, and none has a timeout).
//
// Threading: request() and stop() are the UI thread's. Everything else
// runs on the reader's own thread, which sleeps on a condition variable
// until a reading is asked for: nothing is polled, and with the card closed
// the provider stops asking, so nothing runs at all (TR §8). Asked nothing
// for `idleClose`, it also closes its counters and forgets its last
// samples. The reading is handed to `done` on the reader's thread; the
// provider posts it to the UI thread.

#include "providers/statistics/stats_card.h"
#include "providers/statistics/stats_reading.h"

#include <chrono>
#include <cstddef>
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
    // A rate needs two samples: with none recent enough, the reader takes one, waits this long and takes the reading.
    std::chrono::milliseconds firstSpan{ 250 };
    // A sample older than this isn't "since the last reading" any more (the card was closed meanwhile).
    std::chrono::milliseconds staleAfter{ 10'000 };
    // Asked nothing for this long, the counters are closed.
    std::chrono::milliseconds idleClose{ 30'000 };
    // Bluetooth devices are listed every this many readings (the slowest look, and the levels move slowly), and at once on their page.
    std::size_t bluetoothEvery = 5;
};

class StatsReader {
public:
    using Done = std::function<void(StatsReading)>; // on the reader's thread

    explicit StatsReader(Done done);
    ~StatsReader(); // stop()
    StatsReader(const StatsReader&) = delete;
    StatsReader& operator=(const StatsReader&) = delete;

    // A reading for the card with `page` open; returns at once. Skipped
    // while one is under way (the next request comes in statsRefresh anyway).
    void request(StatsPage page, const StatsReaderSettings& settings);
    // Ends the thread and waits for it: at most a reading's length, and no
    // `done` after it returns.
    void stop();

private:
    struct Sources; // the counters and the last samples: the reader thread's only

    void run();
    StatsReading read(Sources& sources, StatsPage page, const StatsReaderSettings& settings);
    bool waitFor(std::chrono::milliseconds span); // false: stopped meanwhile

    Done done_;

    std::mutex mutex_;
    std::condition_variable wake_;
    std::optional<StatsPage> wanted_; // a request not yet taken
    StatsReaderSettings settings_;    // ...and the settings it came with
    bool reading_ = false;
    bool stopped_ = false;
    std::thread thread_;
};

} // namespace cup
