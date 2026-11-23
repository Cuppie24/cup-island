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
// samples. The answer is handed to `done` on the reader's thread; the
// provider posts it to the UI thread.

#include "providers/statistics/stats_card.h"
#include "providers/statistics/stats_reading.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace cup {

// The reader's own timings. Not in tokens.json yet: they'd be the first
// tokens read off the UI thread, and this change stays out of core/ (decision 173).
struct StatsReaderSettings {
    // A rate needs two samples: with none recent enough, the reader takes one, waits this long and takes the reading.
    std::chrono::milliseconds firstSpan{ 250 };
    // A sample older than this isn't "since the last reading" any more (the card was closed meanwhile).
    std::chrono::milliseconds staleAfter{ 10'000 };
    // Asked nothing for this long, the counters are closed.
    std::chrono::milliseconds idleClose{ 30'000 };
    // Bluetooth devices are listed every this many readings (the slowest look, and the levels move slowly), and at once on their page.
    int bluetoothEvery = 5;
};

struct StatsAnswer {
    StatsReading reading;
    std::string networkDay; // today's network totals as text, for the provider to keep (stats_network.h)
};

class StatsReader {
public:
    using Done = std::function<void(StatsAnswer)>; // on the reader's thread

    // `networkDay`: today's totals as kept by the last run ("" for none).
    StatsReader(Done done, std::string networkDay, StatsReaderSettings settings = {});
    ~StatsReader(); // stop()
    StatsReader(const StatsReader&) = delete;
    StatsReader& operator=(const StatsReader&) = delete;

    // A reading for the card with `page` open; returns at once. Skipped
    // while one is under way (the next request comes in statsRefresh anyway).
    void request(StatsPage page);
    // Ends the thread and waits for it: at most a reading's length, and no
    // `done` after it returns.
    void stop();

private:
    struct Sources; // the counters and the last samples: the reader thread's only

    void run();
    StatsAnswer read(Sources& sources, StatsPage page);
    bool waitFor(std::chrono::milliseconds span); // false: stopped meanwhile

    Done done_;
    StatsReaderSettings settings_;
    std::string networkDay_; // moved into Sources by the thread

    std::mutex mutex_;
    std::condition_variable wake_;
    std::optional<StatsPage> wanted_; // a request not yet taken
    bool reading_ = false;
    bool stopped_ = false;
    std::thread thread_;
};

} // namespace cup
