#pragma once
// Statistics on Windows (decision 173): StatisticsProvider with its reader
// behind it. The pure provider decides when a reading is wanted and what
// the card shows; this starts the reader with it, hands it each request
// with the open page, and posts its readings back to the UI thread.

#include "providers/statistics/statistics_provider.h"
#include "providers/statistics/stats_reader.h"

#include <functional>
#include <memory>

namespace cup {

class SystemStatisticsProvider : public StatisticsProvider {
public:
    using PostToUi = std::function<void(std::function<void()>)>;
    explicit SystemStatisticsProvider(PostToUi postToUi);
    ~SystemStatisticsProvider() override;

    void start(Host& host) override; // starts the reader's thread, asleep until the card is shown
    void stop() override;            // ends it and waits: no reading arrives after

private:
    PostToUi post_;
    std::shared_ptr<bool> alive_; // false after stop(): a reading already posted becomes a no-op
    std::unique_ptr<StatsReader> reader_;
};

} // namespace cup
