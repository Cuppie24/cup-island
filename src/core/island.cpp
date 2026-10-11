#include "core/island.h"

#include "core/alert_queue.h"
#include "core/split.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// --- Helpers --------------------------------------------------------------

Activity* findActivity(IslandState& s, const std::string& id)
{
    for (auto& a : s.activities)
        if (a.id == id) return &a;
    return nullptr;
}


// PS §6.2: the user's pick, then level, then relevance, then most recently
// started. The pick beats every level (decision 007).
void rank(IslandState& s)
{
    std::stable_sort(s.activities.begin(), s.activities.end(), [&](const Activity& a, const Activity& b) {
        bool pa = a.id == s.pinnedId, pb = b.id == s.pinnedId;
        if (pa != pb) return pa;
        if (a.level != b.level) return a.level > b.level;
        if (a.relevance != b.relevance) return a.relevance > b.relevance;
        return a.startOrder > b.startOrder;
    });
}

void expand(IslandState& s, const std::string& id, Ms now)
{
    s.expandedId = id;
    s.homeOpen = false;
    s.lastPointerAt = now;
}

void openHome(IslandState& s, Ms now)
{
    s.expandedId.clear();
    s.homeOpen = true;
    s.lastPointerAt = now;
}

void collapse(IslandState& s)
{
    s.expandedId.clear();
    s.homeOpen = false;
}

// Unsent text in a field: Expanded doesn't time out under it (decision 016).
bool typing(const IslandState& s) { return s.draft && !s.draft->text.empty(); }

// Is what Expanded shows still there? A card stays while its activity or its
// panel exists: a panel's activity ending shows the dormant card (decision 009).
bool expandedViewExists(const IslandState& s)
{
    if (s.homeOpen) return !s.panels.empty();
    return findActivity(s, s.expandedId) || findPanel(s, s.expandedId);
}

// Runs after every event: expire timers, drop stale state, show the next alert.
void settle(IslandState& s, Ms now)
{
    std::erase_if(s.activities, [&](const Activity& a) { return a.expiresAt && now >= *a.expiresAt; });
    if (!s.pinnedId.empty() && !findActivity(s, s.pinnedId)) s.pinnedId.clear(); // a pick dies with its activity

    if (isExpanded(s)) {
        bool timedOut = now >= s.lastPointerAt + tokens().expandedIdleTimeout && !typing(s);
        if (timedOut || !expandedViewExists(s)) collapse(s);
    }
    // A draft lives on the card it was started on: gone with it (decision 016).
    if (s.draft && (s.homeOpen || s.expandedId != s.draft->activityId)) s.draft.reset();

    settleAlerts(s, now); // after Expanded may have closed: alerts it held back can show

    // The bubble left the screen mid-drag (an alert, an activity ended): drop the drag.
    if (s.bubbleDrag && mode(s) != Mode::Split) s.bubbleDrag.reset();
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
        // Home stays: it's where things are started from (decision 009). Cards
        // close (provisional: until expanded content has its own buttons).
        if (!s.homeOpen) collapse(s);
        break;
    case Mode::Idle:
        if (!s.panels.empty()) openHome(s, now); // decision 009
        break;
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

void handle(IslandState& s, const AlertPosted& e, Ms now)
{
    if (postAlert(s, e.alert, now)) collapse(s); // only critical interrupts Expanded
}
void handle(IslandState& s, const PillClicked&, Ms now) { onClick(s, now); }
void handle(IslandState& s, const ClickedOutside&, Ms) { collapse(s); }
void handle(IslandState& s, const EscapePressed&, Ms) { collapse(s); }
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
    collapse(s);
    suspendShowingAlert(s);
    std::erase_if(s.alertQueue, [](const Alert& a) { return a.level < Level::TimeSensitive; });
}

void handle(IslandState&, const Tick&, Ms) {} // settle() does the work

void handle(IslandState& s, const BubbleClicked&, Ms now)
{
    if (mode(s) == Mode::Split) expand(s, s.activities[1].id, now);
    else onClick(s, now); // no bubble on screen (e.g. an alert covers it): an ordinary click
}

void handle(IslandState& s, const ActivityPromoted& e, Ms now)
{
    if (!findActivity(s, e.id)) return;
    s.pinnedId = e.id;
    rank(s);
    if (isExpanded(s)) s.lastPointerAt = now; // the user is busy here
}

void handle(IslandState& s, const BubbleDragged& e, Ms)
{
    if (mode(s) != Mode::Split) return;
    if (s.bubbleDrag || isDrag(e.drag)) s.bubbleDrag = e.drag; // small wobbles stay a click
}

void handle(IslandState& s, const BubbleReleased&, Ms now)
{
    bool swap = swapPreview(s);
    s.bubbleDrag.reset();
    if (swap) handle(s, ActivityPromoted{ s.activities[1].id }, now);
}

void handle(IslandState& s, const BubbleDragCancelled&, Ms) { s.bubbleDrag.reset(); }

void handle(IslandState& s, const ActivitySwitched& e, Ms now)
{
    if (!findActivity(s, e.id)) return;
    handle(s, ActivityPromoted{ e.id }, now);
    if (isExpanded(s)) expand(s, e.id, now);
}

void handle(IslandState& s, const PanelPublished& e, Ms)
{
    for (Panel& p : s.panels) {
        if (p.id == e.panel.id) {
            p = e.panel; // keeps its place on Home
            return;
        }
    }
    s.panels.push_back(e.panel);
}

void handle(IslandState& s, const PanelRemoved& e, Ms)
{
    std::erase_if(s.panels, [&](const Panel& p) { return p.id == e.id; });
}

void handle(IslandState& s, const HomeOpened&, Ms now)
{
    if (!s.panels.empty()) openHome(s, now);
}

// A running panel opens like its tab (and becomes the pick); a dormant one shows its card.
void handle(IslandState& s, const TileClicked& e, Ms now)
{
    if (findActivity(s, e.id)) {
        handle(s, ActivityPromoted{ e.id }, now);
        expand(s, e.id, now);
    } else if (const Panel* p = findPanel(s, e.id); p && p->tapAction.empty() && hasCard(p->content)) {
        expand(s, e.id, now); // static widgets have no card; a toggle's tap goes to its provider
    }
}

// --- Text input (decision 016) ------------------------------------------------

void handle(IslandState& s, const InputFocused& e, Ms now)
{
    if (s.homeOpen || s.expandedId != e.activityId) return; // only on the card that's shown
    if (!s.draft || s.draft->actionId != e.actionId) s.draft = Draft{ e.providerId, e.activityId, e.actionId, "" };
    s.lastPointerAt = now;
}

void handle(IslandState& s, const TextTyped& e, Ms now)
{
    if (!s.draft) return;
    s.draft->text += e.text;
    s.lastPointerAt = now; // the user is busy here
}

void handle(IslandState& s, const TextErased&, Ms now)
{
    if (!s.draft) return;
    std::string& text = s.draft->text;
    while (!text.empty()) { // drop one UTF-8 character: continuation bytes, then its lead byte
        bool continuation = (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80;
        text.pop_back();
        if (!continuation) break;
    }
    s.lastPointerAt = now;
}

void handle(IslandState& s, const InputSubmitted&, Ms now)
{
    if (!s.draft) return;
    s.draft->text.clear(); // focus stays: add the next one
    s.lastPointerAt = now;
}

void handle(IslandState& s, const InputCancelled&, Ms) { s.draft.reset(); }

void handle(IslandState& s, const ClockSynced& e, Ms now) { s.clock = WallClock{ e.timeOfDay - now, e.hour24, e.date }; }

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

bool swapPreview(const IslandState& s)
{
    return s.bubbleDrag && mode(s) == Mode::Split && bubbleOverPill(*s.bubbleDrag);
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
    for (const auto& a : s.activities)
        if (a.expiresAt) consider(*a.expiresAt);
    if (isExpanded(s) && !typing(s)) consider(s.lastPointerAt + tokens().expandedIdleTimeout);
    for (const auto& a : s.alertQueue) consider(a.postedAt + tokens().heldAlertMaxAge + Ms{ 1 });
    return next;
}


} // namespace cup
