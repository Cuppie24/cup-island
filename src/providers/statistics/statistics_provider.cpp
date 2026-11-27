#include "providers/statistics/statistics_provider.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {

void StatisticsProvider::start(Host& host)
{
    host_ = &host;
    shown_ = live_ = false;
    listedAt_.reset();
    view_ = {};
    view_.picks = loadPicks(host_->load(kPicksKey));
    host_->publishPanel(statisticsPanel(view_));
    if (wanted()) ask(); // picked readings: the pill fills as soon as the first arrives
}

void StatisticsProvider::stop()
{
    if (!host_) return;
    host_->wakeAt(std::nullopt);
    if (live_) host_->endActivity(kPanelId);
    host_->removePanel(kPanelId);
    host_ = nullptr;
    shown_ = live_ = false;
}

// A pill with something in it is an activity; without, the page is dormant
// (decision 009). Passive and least relevant: under media and the timer,
// whatever started last (PS §6.2).
void StatisticsProvider::show()
{
    Panel panel = statisticsPanel(view_);
    if (hasPill(panel.content)) {
        host_->startActivity({ .id = kPanelId, .level = Level::Passive, .relevance = 0.0f, .content = panel.content });
        live_ = true;
        return;
    }
    if (live_) host_->endActivity(kPanelId);
    live_ = false;
    host_->publishPanel(std::move(panel));
}

void StatisticsProvider::forget()
{
    view_.reading = {};
    view_.devices.clear();
    listedAt_.reset();
}

void StatisticsProvider::ask()
{
    const Tokens& t = tokens();
    Ms now = host_->now();
    StatsRequest readings = shown_ ? cardRequest(false) : pillRequest(view_.picks, false);
    bool devices = shown_ || devicePicked(view_.picks);
    // Bluetooth is the look to keep rare (decision 042): every statsBluetoothRefresh with the card shown;
    // for the pill once a minute. Woken only for it (nothing else picked), that wake is the minute's.
    bool onlyDevices = devices && !readings.any();
    Ms every = shown_ ? t.statsBluetoothRefresh : onlyDevices ? t.statsPillBluetoothRefresh / 2 : t.statsPillBluetoothRefresh;
    bool due = devices && (!listedAt_ || now - *listedAt_ >= every);
    StatsRequest request = shown_ ? cardRequest(due) : pillRequest(view_.picks, due);
    if (due) listedAt_ = now;
    if (request_ && request.any()) request_(request);

    // Never a zero or negative wait from a mistyped token: at least a millisecond on.
    if (shown_ || readings.any()) {
        host_->wakeAt(now + std::max(shown_ ? t.statsRefresh : t.statsPillRefresh, Ms{ 1 }));
    } else if (onlyDevices) {
        // On the wall clock's minute, when the idle island wakes for its clock anyway (TR §8): one wake-up, not two.
        Ms period = std::max(t.statsPillBluetoothRefresh, Ms{ 1 });
        host_->wakeAt(now + period - host_->wallClock() % period);
    } else {
        host_->wakeAt(std::nullopt);
    }
}

void StatisticsProvider::onCardShown(const std::string& activityId)
{
    if (!host_ || activityId != kPanelId) return;
    shown_ = true;
    ask();
}

void StatisticsProvider::onWake()
{
    if (host_ && wanted()) ask();
}

// Closed with nothing picked, nobody is looking: it stops asking, and
// forgets, rather than show a load from an hour ago as now's next time.
// With readings picked it goes on, asking only for those, less often.
void StatisticsProvider::onCardClosed(const std::string& activityId)
{
    if (!host_ || activityId != kPanelId) return;
    shown_ = false;
    if (wanted()) {
        host_->wakeAt(host_->now() + std::max(tokens().statsPillRefresh, Ms{ 1 }));
        return;
    }
    host_->wakeAt(std::nullopt);
    forget();
    show();
}

void StatisticsProvider::publish(const StatsReading& reading)
{
    if (!host_ || !wanted()) return;
    view_.reading = reading;
    if (reading.bluetooth) { // listed this time: the listing until the next
        view_.devices = *reading.bluetooth;
        if (rememberDevices(view_.picks, view_.devices)) { // a device Settings hasn't listed yet
            host_->save(kPicksKey, savePicks(view_.picks));
            if (changed_) changed_();
        }
    }
    show();
}

void StatisticsProvider::toggleChoice(const std::string& id)
{
    if (!host_) return;
    bool was = wanted();
    if (!togglePick(view_.picks, id)) return;
    host_->save(kPicksKey, savePicks(view_.picks));
    if (!wanted()) { // the last pick went, with the card closed: as when it closes
        host_->wakeAt(std::nullopt);
        forget();
    }
    show();
    // The first pick with the card closed starts the readings; one more, shown or not, is read with the next.
    if (!was && wanted()) ask();
}

} // namespace cup
