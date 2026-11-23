#pragma once
// Statistics: the machine's vital signs on a page of tiles, and in the pill
// the readings picked in Settings (decisions 171, 174). Pure: it reads
// nothing itself. It says when a reading is wanted and of what
// (`requestReading`, a StatsRequest) and is handed one (`publish`); the
// Windows reader behind those two is separate, off the UI thread (decision 144).
//
// What is asked, and when (all through Host::wakeAt; TR §8):
//  - its card shown: everything on it, every statsRefresh; Bluetooth devices
//    every statsBluetoothRefresh;
//  - its card closed, readings picked for the pill: only those, every
//    statsPillRefresh; a picked device's battery every
//    statsPillBluetoothRefresh (a minute), and with nothing else picked the
//    wake-up is on the minute, with the idle clock's;
//  - its card closed, nothing picked: nothing, and nothing is watched
//    (decision 042: no standing Bluetooth watcher).
// With a picked reading to show it is a running activity, ranked under
// every other (passive): it never takes the pill from a song or a timer.

#include "core/provider.h"
#include "providers/statistics/stats_card.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace cup {

class StatisticsProvider : public Provider {
public:
    // Called on the UI thread each time a reading is wanted, with what it
    // should hold. It must return at once: start the read elsewhere, and
    // call publish() on the UI thread when it's done. Asked again while a
    // read is still under way, the reader may skip it. Empty: nothing is read (tests).
    using RequestReading = std::function<void(const StatsRequest&)>;
    explicit StatisticsProvider(RequestReading requestReading = {}) : request_(std::move(requestReading)) {}

    const char* id() const override { return "statistics"; }
    void start(Host& host) override; // publishes its page, empty; with readings picked, starts asking for them
    void stop() override;
    void onAction(const Action&) override {} // nothing on the card to click
    void onWake() override;                                      // the next reading is due
    void onCardShown(const std::string& activityId) override;    // everything, now
    void onCardClosed(const std::string& activityId) override;   // only the pill's from here; nothing picked: stops, and forgets

    // The reader's answer, on the UI thread: shows it. One that nobody is
    // waiting for (the card closed and nothing picked, the provider stopped) is dropped.
    void publish(const StatsReading& reading);

    // Settings' Statistics page (decision 174): its switches, and one flipped.
    // `onChoicesChanged`: called when the list changes by itself (a new
    // Bluetooth device was seen), so Settings can show it.
    std::vector<StatsChoice> choices() const { return choicesOf(view_.picks); }
    void toggleChoice(const std::string& id);
    void onChoicesChanged(std::function<void()> changed) { changed_ = std::move(changed); }

    static constexpr const char* kPanelId = kStatisticsPanelId;
    static constexpr const char* kPicksKey = "pill"; // Host::load/save: savePicks' text

private:
    bool wanted() const { return shown_ || !view_.picks.picked.empty(); } // someone is waiting for readings
    void ask();  // a reading now, and a wake-up for the next
    void show(); // the view on its card and in the pill
    void forget(); // the readings it has: nobody is looking, and they'd be old when someone does

    RequestReading request_;
    std::function<void()> changed_;
    Host* host_ = nullptr;
    bool shown_ = false;
    bool live_ = false; // its activity is running: the pill has something to show
    std::optional<Ms> listedAt_; // when the Bluetooth devices were last asked for
    StatsView view_;
};

} // namespace cup
