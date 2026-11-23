#pragma once
// Statistics: a page of the machine's vital signs (decisions 171, 172).
// Pure: it reads nothing itself. It says when a reading is wanted
// (`requestReading`) and is handed one (`publish`); the Windows reader
// behind those two is separate, off the UI thread (decision 144).
// Readings are asked for only while its card is shown, statsRefresh apart,
// through Host::wakeAt: closed, nothing runs and nothing is watched (TR §8;
// decision 042: no standing Bluetooth watcher). A page only: it never
// starts an activity, so it's never ranked into the pill.
// It keeps what the card shows (StatsView): the last reading, each graph's
// history, and which of its pages is open (the overview, or a tile's own).

#include "core/provider.h"
#include "providers/statistics/stats_card.h"

#include <functional>
#include <string>

namespace cup {

class StatisticsProvider : public Provider {
public:
    // Called on the UI thread each time a new reading is wanted. It must
    // return at once: start the read elsewhere, and call publish() on the UI
    // thread when it's done. Asked again while a read is still under way,
    // the reader may skip it. Empty: the card stays empty (tests).
    using RequestReading = std::function<void()>;
    explicit StatisticsProvider(RequestReading requestReading = {}) : request_(std::move(requestReading)) {}

    const char* id() const override { return "statistics"; }
    void start(Host& host) override; // publishes its page, empty
    void stop() override;
    void onAction(const Action& action) override;                // a tile: its page; the arrow or Esc: back
    void onWake() override;                                      // statsRefresh passed: the next reading
    void onCardShown(const std::string& activityId) override;    // asks for a reading now, then every statsRefresh
    void onCardClosed(const std::string& activityId) override;   // stops asking; the overview again, empty

    // The reader's answer, on the UI thread: shows it, and adds it to the
    // histories. One that comes with the card closed (a slow read) or the
    // provider stopped is dropped.
    void publish(const StatsReading& reading);

    StatsPage page() const { return view_.page; } // which of its pages is open (for the reader: what's worth reading)

    static constexpr const char* kPanelId = kStatisticsPanelId;

private:
    void ask();  // a reading now, and a wake-up for the next
    void show(); // the view on its card

    RequestReading request_;
    Host* host_ = nullptr;
    bool shown_ = false;
    StatsView view_;
};

} // namespace cup
