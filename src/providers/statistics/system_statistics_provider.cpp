#include "providers/statistics/system_statistics_provider.h"

#include "core/tokens.h"

#include <utility>

namespace cup {

SystemStatisticsProvider::SystemStatisticsProvider(PostToUi postToUi)
    // Asked on the UI thread, where tokens() is read; the reader returns at once.
    : StatisticsProvider([this](const StatsRequest& request) {
          if (!reader_) return;
          const Tokens& t = tokens();
          reader_->request(request, { .firstSpan = t.statsFirstSpan, .staleAfter = t.statsStaleAfter, .idleClose = t.statsIdleClose });
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
    alive_ = std::make_shared<bool>(true);
    // The reader answers on its own thread: each reading is posted here, and
    // dropped if the provider stopped meanwhile (alive_ is only read and
    // written on this thread, as media's is). Before the base's start():
    // with readings picked for the pill it asks for one at once.
    reader_ = std::make_unique<StatsReader>([this, post = post_, alive = alive_](StatsReading reading) {
        post([this, alive, reading = std::move(reading)] {
            if (*alive) publish(reading); // drops it itself if nobody is waiting any more
        });
    });
    StatisticsProvider::start(host);
}

void SystemStatisticsProvider::stop()
{
    if (alive_) *alive_ = false;
    reader_.reset(); // waits for a reading under way: local calls, a fraction of a second
    StatisticsProvider::stop();
}

} // namespace cup
