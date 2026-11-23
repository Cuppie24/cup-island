#include "providers/statistics/statistics_provider.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {

void StatisticsProvider::start(Host& host)
{
    host_ = &host;
    shown_ = false;
    reading_ = {};
    host_->publishPanel(statisticsPanel(reading_));
}

void StatisticsProvider::stop()
{
    if (!host_) return;
    host_->wakeAt(std::nullopt);
    host_->removePanel(kPanelId);
    host_ = nullptr;
    shown_ = false;
}

void StatisticsProvider::ask()
{
    if (request_) request_();
    // Never a zero or negative wait from a mistyped token: at least a millisecond on.
    host_->wakeAt(host_->now() + std::max(tokens().statsRefresh, Ms{ 1 }));
}

void StatisticsProvider::onCardShown(const std::string& activityId)
{
    if (!host_ || activityId != kPanelId) return;
    shown_ = true;
    ask();
}

void StatisticsProvider::onWake()
{
    if (host_ && shown_) ask();
}

// Closed, the numbers on it would be old by the time it's opened again:
// it goes back to "—" rather than show a load from an hour ago as now's.
void StatisticsProvider::onCardClosed(const std::string& activityId)
{
    if (!host_ || activityId != kPanelId) return;
    shown_ = false;
    host_->wakeAt(std::nullopt);
    if (reading_ == StatsReading{}) return;
    reading_ = {};
    host_->publishPanel(statisticsPanel(reading_));
}

void StatisticsProvider::publish(const StatsReading& reading)
{
    if (!host_ || !shown_) return;
    // Compared as shown: a reading that rounds to the same card publishes nothing, so nothing is redrawn.
    Panel next = statisticsPanel(reading);
    bool same = next == statisticsPanel(reading_);
    reading_ = reading;
    if (!same) host_->publishPanel(std::move(next));
}

} // namespace cup
