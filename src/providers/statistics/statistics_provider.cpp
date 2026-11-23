#include "providers/statistics/statistics_provider.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {

void StatisticsProvider::start(Host& host)
{
    host_ = &host;
    shown_ = false;
    view_ = {};
    show();
}

void StatisticsProvider::stop()
{
    if (!host_) return;
    host_->wakeAt(std::nullopt);
    host_->removePanel(kPanelId);
    host_ = nullptr;
    shown_ = false;
}

void StatisticsProvider::show()
{
    host_->publishPanel(statisticsPanel(view_));
}

void StatisticsProvider::ask()
{
    if (request_) request_();
    // Never a zero or negative wait from a mistyped token: at least a millisecond on.
    host_->wakeAt(host_->now() + std::max(tokens().statsRefresh, Ms{ 1 }));
}

void StatisticsProvider::onAction(const Action& action)
{
    if (!host_) return;
    StatsPage page = view_.page;
    if (action.actionId == stats_action::kBack) page = StatsPage::Overview;
    // Only a page the overview has a tile for: no GPU, no GPU page.
    else if (auto opened = pageOpenedBy(action.actionId); opened && hasPage(view_.reading, *opened)) page = *opened;
    if (page == view_.page) return;
    view_.page = page;
    show();
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

// Closed, the numbers on it would be old by the time it's opened again, and
// the graphs would have a hole as long as it was closed: it goes back to an
// empty overview rather than show a load from an hour ago as now's.
void StatisticsProvider::onCardClosed(const std::string& activityId)
{
    if (!host_ || activityId != kPanelId) return;
    shown_ = false;
    host_->wakeAt(std::nullopt);
    if (view_ == StatsView{}) return;
    view_ = {};
    show();
}

void StatisticsProvider::publish(const StatsReading& reading)
{
    if (!host_ || !shown_) return;
    view_.reading = reading;
    record(view_.history, reading);
    // What its page showed is gone (the GPU's counters stopped): back to the overview, which has no tile for it either.
    if (!hasPage(reading, view_.page)) view_.page = StatsPage::Overview;
    show();
}

} // namespace cup
