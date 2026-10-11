#include "core/island.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// --- Helpers --------------------------------------------------------------

bool isExpanded(const IslandState& s) { return !s.expandedId.empty(); }

Activity* findActivity(IslandState& s, const std::string& id)
{
    for (auto& a : s.activities)
        if (a.id == id) return &a;
    return nullptr;
}

// PS §6.2: level, then relevance, then most recently started.
void rank(IslandState& s)
{
    std::stable_sort(s.activities.begin(), s.activities.end(), [](const Activity& a, const Activity& b) {
        if (a.level != b.level) return a.level > b.level;
        if (a.relevance != b.relevance) return a.relevance > b.relevance;
        return a.startOrder > b.startOrder;
    });
}

void expand(IslandState& s, const std::string& id, Ms now)
{
    s.expandedId = id;
    s.lastPointerAt = now;
}

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

// An overlay is taking over: put the showing alert back to replay later, or
// drop it if the island is hidden and the alert isn't urgent (PS §6.4).
void suspendShowingAlert(IslandState& s)
{
    if (!s.alert) return;
    bool keep = !s.hidden || s.alert->level >= Level::TimeSensitive;
    if (keep) s.alertQueue.push_front(std::move(*s.alert));
    s.alert.reset();
}

void postAlert(IslandState& s, Alert alert, Ms now)
{
    alert.postedAt = now;
    if (alert.level == Level::Passive) return; // passive never alerts
    if (s.hidden && alert.level < Level::TimeSensitive) return;

    if (!alert.coalesceKey.empty()) {
        if (s.alert && s.alert->coalesceKey == alert.coalesceKey) {
            s.alert = std::move(alert);
            s.alertUntil = now + tokens().alertDuration;
            return;
        }
        for (auto& queued : s.alertQueue) {
            if (queued.coalesceKey == alert.coalesceKey) {
                queued = std::move(alert);
                return;
            }
        }
    }

    if (alert.level == Level::Critical) s.expandedId.clear(); // only critical interrupts Expanded

    if (s.alert && alert.level > s.alert->level) {
        s.alertQueue.push_front(std::move(*s.alert)); // pre-empted, replays next
        s.alert = std::move(alert);
        s.alertUntil = now + tokens().alertDuration;
        return;
    }
    enqueue(s, std::move(alert));
}

// Runs after every event: expire timers, drop stale state, show the next alert.
void settle(IslandState& s, Ms now)
{
    if (s.alert && now >= s.alertUntil) s.alert.reset();

    if (isExpanded(s)) {
        bool timedOut = now >= s.lastPointerAt + tokens().expandedIdleTimeout;
        if (timedOut || !findActivity(s, s.expandedId)) s.expandedId.clear();
    }

    std::erase_if(s.alertQueue, [&](const Alert& a) { return now - a.postedAt > tokens().heldAlertMaxAge; });

    if (!s.alert) {
        auto next = std::find_if(s.alertQueue.begin(), s.alertQueue.end(),
            [&](const Alert& a) { return alertAllowed(s, a.level); });
        if (next != s.alertQueue.end()) {
            s.alert = std::move(*next);
            s.alertQueue.erase(next);
            s.alertUntil = now + tokens().alertDuration;
        }
    }
}

void onClick(IslandState& s, Ms now)
{
    switch (mode(s)) {
    case Mode::Alert: {
        // Clicking an alert opens its activity, or the top one if it has none.
        std::string target = findActivity(s, s.alert->activityId) ? s.alert->activityId : "";
        if (target.empty() && !s.activities.empty()) target = s.activities.front().id;
        s.alert.reset();
        if (!target.empty()) expand(s, target, now);
        break;
    }
    case Mode::Compact:
    case Mode::Split:
        expand(s, s.activities.front().id, now);
        break;
    case Mode::Expanded:
        s.expandedId.clear(); // provisional: until expanded content has its own buttons
        break;
    case Mode::Idle:
    case Mode::Receptive:
    case Mode::Hidden:
        break;
    }
}

// --- Event handlers -------------------------------------------------------

void handle(IslandState& s, const ActivityStarted& e, Ms)
{
    if (Activity* existing = findActivity(s, e.activity.id)) {
        auto order = existing->startOrder;
        *existing = e.activity;
        existing->startOrder = order;
    } else {
        s.activities.push_back(e.activity);
        s.activities.back().startOrder = s.nextStartOrder++;
    }
    rank(s);
}

void handle(IslandState& s, const ActivityUpdated& e, Ms)
{
    if (Activity* a = findActivity(s, e.id)) a->content = e.content;
}

void handle(IslandState& s, const ActivityEnded& e, Ms)
{
    std::erase_if(s.activities, [&](const Activity& a) { return a.id == e.id; });
}

void handle(IslandState& s, const AlertPosted& e, Ms now) { postAlert(s, e.alert, now); }
void handle(IslandState& s, const PillClicked&, Ms now) { onClick(s, now); }
void handle(IslandState& s, const ClickedOutside&, Ms) { s.expandedId.clear(); }
void handle(IslandState& s, const EscapePressed&, Ms) { s.expandedId.clear(); }
void handle(IslandState& s, const PointerMoved&, Ms now) { s.lastPointerAt = now; }

void handle(IslandState& s, const DragEntered& e, Ms)
{
    if (!e.accepted) return;
    s.dragging = true;
    suspendShowingAlert(s);
}

void handle(IslandState& s, const DragLeft&, Ms) { s.dragging = false; }
void handle(IslandState& s, const Dropped&, Ms) { s.dragging = false; }

void handle(IslandState& s, const HiddenChanged& e, Ms)
{
    s.hidden = e.hidden;
    if (!e.hidden) return;
    s.expandedId.clear();
    suspendShowingAlert(s);
    std::erase_if(s.alertQueue, [](const Alert& a) { return a.level < Level::TimeSensitive; });
}

void handle(IslandState&, const Tick&, Ms) {} // settle() does the work

} // namespace

// --- Public API -------------------------------------------------------------

IslandState reduce(IslandState state, const Event& event, Ms now)
{
    std::visit([&](const auto& e) { handle(state, e, now); }, event);
    settle(state, now);
    return state;
}

Mode restingMode(const IslandState& s)
{
    if (s.activities.empty()) return Mode::Idle;
    if (s.activities.size() == 1) return Mode::Compact;
    return Mode::Split;
}

Mode mode(const IslandState& s)
{
    if (s.hidden) return Mode::Hidden;
    if (s.dragging) return Mode::Receptive;
    if (isExpanded(s)) return Mode::Expanded;
    if (s.alert) return Mode::Alert;
    return restingMode(s);
}

std::optional<Ms> nextDeadline(const IslandState& s)
{
    std::optional<Ms> next;
    auto consider = [&](Ms t) { if (!next || t < *next) next = t; };

    if (s.alert) consider(s.alertUntil);
    if (isExpanded(s)) consider(s.lastPointerAt + tokens().expandedIdleTimeout);
    for (const auto& a : s.alertQueue) consider(a.postedAt + tokens().heldAlertMaxAge + Ms{ 1 });
    return next;
}

const char* toString(Mode mode)
{
    switch (mode) {
    case Mode::Idle: return "Idle";
    case Mode::Compact: return "Compact";
    case Mode::Split: return "Split";
    case Mode::Alert: return "Alert";
    case Mode::Expanded: return "Expanded";
    case Mode::Receptive: return "Receptive";
    case Mode::Hidden: return "Hidden";
    }
    return "?";
}

const char* toString(Level level)
{
    switch (level) {
    case Level::Passive: return "passive";
    case Level::Active: return "active";
    case Level::TimeSensitive: return "timeSensitive";
    case Level::Critical: return "critical";
    }
    return "?";
}

} // namespace cup
