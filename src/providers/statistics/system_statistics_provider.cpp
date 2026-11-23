#include "providers/statistics/system_statistics_provider.h"

#include <utility>

namespace cup {

SystemStatisticsProvider::SystemStatisticsProvider(PostToUi postToUi)
    // Asked on the UI thread, where page() is read; the reader returns at once.
    : StatisticsProvider([this] {
          if (reader_) reader_->request(page());
      }),
      post_(std::move(postToUi))
{
}

SystemStatisticsProvider::~SystemStatisticsProvider()
{
    if (alive_) *alive_ = false;
    reader_.reset(); // joins its thread before the rest of this goes
}

void SystemStatisticsProvider::start(Host& host)
{
    host_ = &host;
    networkDay_ = savedDay_ = host.load(kNetworkDayKey);
    alive_ = std::make_shared<bool>(true);
    // The reader answers on its own thread: each answer is posted here, and
    // dropped if the provider stopped meanwhile (alive_ is only read and
    // written on this thread, as media's is).
    reader_ = std::make_unique<StatsReader>(
        [this, post = post_, alive = alive_](StatsAnswer answer) {
            post([this, alive, answer = std::move(answer)] {
                if (!*alive) return;
                if (!answer.networkDay.empty()) networkDay_ = answer.networkDay;
                publish(answer.reading); // drops it itself if the card closed meanwhile
            });
        },
        networkDay_);
    StatisticsProvider::start(host);
}

void SystemStatisticsProvider::stop()
{
    if (alive_) *alive_ = false;
    reader_.reset(); // waits for a reading under way: local calls, a fraction of a second
    if (host_) keepNetworkDay();
    host_ = nullptr;
    StatisticsProvider::stop();
}

void SystemStatisticsProvider::onCardClosed(const std::string& activityId)
{
    StatisticsProvider::onCardClosed(activityId);
    // Not at every reading (a file written every two seconds): when the
    // card closes. The counters go on meanwhile, so nothing is lost between.
    if (host_ && activityId == kPanelId) keepNetworkDay();
}

void SystemStatisticsProvider::keepNetworkDay()
{
    if (networkDay_ == savedDay_) return;
    host_->save(kNetworkDayKey, networkDay_);
    savedDay_ = networkDay_;
}

} // namespace cup
