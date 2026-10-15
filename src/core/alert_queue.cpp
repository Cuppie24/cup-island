#include "core/alert_queue.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// Can an alert of this level be on screen right now? PS §3.3, §6.4.
bool alertAllowed(const IslandState& s, Level level)
{
    if (s.hidden || s.dragging) return false;
    if (isExpanded(s)) return level == Level::Critical;
    return true;
}

// Queue full: drop the oldest alert of the lowest level present (PS §6.3).
void enqueue(IslandState& s, Alert alert)
{
    if (s.alertQueue.size() >= tokens().alertQueueCapacity) {
        auto lowest = std::min_element(s.alertQueue.begin(), s.alertQueue.end(),
            [](const Alert& a, const Alert& b) { return a.level < b.level; });
        s.alertQueue.erase(lowest);
    }
    s.alertQueue.push_back(std::move(alert));
}

} // namespace

void suspendShowingAlert(IslandState& s)
{
    if (!s.alert) return;
    bool keep = !s.hidden || s.alert->level >= Level::TimeSensitive;
    if (keep) s.alertQueue.push_front(std::move(*s.alert));
    s.alert.reset();
}

bool postAlert(IslandState& s, Alert alert, Ms now)
{
    alert.postedAt = now;
    if (alert.refreshOnly) {
        auto refresh = [&](Alert& shown) {
            if (!alert.coalesceKey.empty() && shown.coalesceKey == alert.coalesceKey) shown.content = alert.content;
        };
        if (s.alert) refresh(*s.alert);
        for (auto& queued : s.alertQueue) refresh(queued);
        return false;
    }
    if (alert.level == Level::Passive) return false; // passive never alerts
    if (s.hidden && alert.level < Level::TimeSensitive) return false;

    if (!alert.coalesceKey.empty()) {
        if (s.alert && s.alert->coalesceKey == alert.coalesceKey) {
            s.alert = std::move(alert);
            s.alertUntil = now + tokens().alertDuration;
            return false;
        }
        for (auto& queued : s.alertQueue) {
            if (queued.coalesceKey == alert.coalesceKey) {
                queued = std::move(alert);
                return false;
            }
        }
    }

    bool critical = alert.level == Level::Critical;
    if (s.alert && alert.level > s.alert->level) {
        s.alertQueue.push_front(std::move(*s.alert)); // pre-empted, replays next
        s.alert = std::move(alert);
        s.alertUntil = now + tokens().alertDuration;
        return critical;
    }
    enqueue(s, std::move(alert));
    return critical;
}

void settleAlerts(IslandState& s, Ms now)
{
    if (s.alert && now >= s.alertUntil) s.alert.reset();
    std::erase_if(s.alertQueue, [&](const Alert& a) { return now - a.postedAt > tokens().heldAlertMaxAge; });
    if (s.alert) return;
    auto next = std::find_if(s.alertQueue.begin(), s.alertQueue.end(), [&](const Alert& a) { return alertAllowed(s, a.level); });
    if (next == s.alertQueue.end()) return;
    s.alert = std::move(*next);
    s.alertQueue.erase(next);
    s.alertUntil = now + tokens().alertDuration;
}

} // namespace cup
