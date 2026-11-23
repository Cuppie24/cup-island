#pragma once
// Statistics: a page of the machine's vital signs (decision 171). Pure: it
// reads nothing itself. It says when a reading is wanted (`requestReading`)
// and is handed one (`publish`); the Windows reader behind those two is
// separate, off the UI thread (decision 144).
// Readings are asked for only while its card is shown, statsRefresh apart,
// through Host::wakeAt: closed, nothing runs and nothing is watched (TR §8;
// decision 042: no standing Bluetooth watcher). A page only: it never
// starts an activity, so it's never ranked into the pill.

#include "core/provider.h"
#include "providers/statistics/stats_card.h"
#include "providers/statistics/stats_reading.h"

#include <functional>
#include <string>

namespace cup {

class StatisticsProvider : public Provider {
public:
    // Called on the UI thread each time a new reading is wanted. It must
    // return at once: start the read elsewhere, and call publish() on the UI
    // thread when it's done. Asked again while a read is still under way,
    // the reader may skip it. Empty: the card stays at "—" (tests).
    using RequestReading = std::function<void()>;
    explicit StatisticsProvider(RequestReading requestReading = {}) : request_(std::move(requestReading)) {}

    const char* id() const override { return "statistics"; }
    void start(Host& host) override; // publishes its page, empty
    void stop() override;
    void onAction(const Action&) override {} // nothing on the card to click
    void onWake() override;                                      // statsRefresh passed: the next reading
    void onCardShown(const std::string& activityId) override;    // asks for a reading now, then every statsRefresh
    void onCardClosed(const std::string& activityId) override;   // stops asking; the card goes back to "—"

    // The reader's answer, on the UI thread: shows it. One that comes with
    // the card closed (a slow read) or the provider stopped is dropped.
    void publish(const StatsReading& reading);

    static constexpr const char* kPanelId = kStatisticsPanelId;

private:
    void ask(); // a reading now, and a wake-up for the next

    RequestReading request_;
    Host* host_ = nullptr;
    bool shown_ = false;
    StatsReading reading_; // what the card shows
};

} // namespace cup
