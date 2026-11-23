#pragma once
// Statistics on Windows (decision 173): StatisticsProvider with its reader
// behind it. The pure provider decides when a reading is wanted and what
// the card shows; this starts the reader with it, hands it each request
// with the open page, and posts its answers back to the UI thread.
// It also keeps today's network totals between runs (Host::save).

#include "providers/statistics/statistics_provider.h"
#include "providers/statistics/stats_reader.h"

#include <functional>
#include <memory>
#include <string>

namespace cup {

class SystemStatisticsProvider : public StatisticsProvider {
public:
    using PostToUi = std::function<void(std::function<void()>)>;
    explicit SystemStatisticsProvider(PostToUi postToUi);
    ~SystemStatisticsProvider() override;

    void start(Host& host) override; // starts the reader's thread, asleep until the card is shown
    void stop() override;            // ends it and waits: no answer arrives after
    void onCardClosed(const std::string& activityId) override; // ...and today's totals are saved

    static constexpr const char* kNetworkDayKey = "network-day"; // today's network totals (stats_network.h's text)

private:
    void keepNetworkDay(); // saved if it changed since the last save

    PostToUi post_;
    Host* host_ = nullptr;
    std::shared_ptr<bool> alive_; // false after stop(): an answer already posted becomes a no-op
    std::unique_ptr<StatsReader> reader_;
    std::string networkDay_; // as the last answer gave it
    std::string savedDay_;
};

} // namespace cup
