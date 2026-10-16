#include "core/island.h"

#include "core/alert_queue.h"
#include "core/card_overlays.h"
#include "core/drag.h"
#include "core/lift.h"
#include "core/lists.h"
#include "core/selection.h"
#include "core/split.h"
#include "core/text_edit.h"
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
    s.cardFromHome = false; // opened some other way; TileClicked sets it after
    s.lastPointerAt = now;
}

void openHome(IslandState& s, Ms now)
{
    s.expandedId.clear();
    s.homeOpen = true;
    s.cardFromHome = false;
    s.lastPointerAt = now;
}

void collapse(IslandState& s)
{
    s.expandedId.clear();
    s.homeOpen = false;
    s.cardFromHome = false;
    s.stayOpen = false; // a pin lasts while it's open (decision 046)
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

// The card shown asks to stay open on a click between its controls (Content::keepOpen).
bool shownCardKeepsOpen(const IslandState& s)
{
    if (const Activity* a = findActivity(s, s.expandedId)) return a->content.keepOpen;
    const Panel* p = findPanel(s, s.expandedId);
    return p && p->content.keepOpen;
}

// Runs after every event: expire timers, drop stale state, show the next alert.
void settle(IslandState& s, Ms now)
{
    std::erase_if(s.activities, [&](const Activity& a) { return a.expiresAt && now >= *a.expiresAt; });
    if (!s.pinnedId.empty() && !findActivity(s, s.pinnedId)) s.pinnedId.clear(); // a pick dies with its activity

    if (isExpanded(s)) {
        // Not under a drag: the card is still there when it leaves (decision 061).
        bool timedOut = now >= s.lastPointerAt + tokens().expandedIdleTimeout && !typing(s) && !s.stayOpen && !s.dragging;
        if (timedOut || !expandedViewExists(s)) collapse(s);
    }
    dropStaleOverlays(s); // a draft or dialog goes with its card
    settleSelection(s);   // a selection goes with its card (decision 064)
    settleDrag(s, now);   // a drag that left and didn't come back
    if (s.criticalHeld && !s.dragging) {
        s.criticalHeld = false;
        collapse(s);
    }

    settleAlerts(s, now); // after Expanded may have closed: alerts it held back can show

    // The bubble left the screen mid-drag (an alert, an activity ended): drop the drag.
    if (s.bubbleDrag && mode(s) != Mode::Split) s.bubbleDrag.reset();
    settleLift(s, now); // a held press lifts; a lift that can't stay lifted lands (decision 026)
}

void onClick(IslandState& s, Ms now)
{
    switch (mode(s)) {
    case Mode::Alert: {
        // Clicking an alert opens its activity, or the card of the panel it
        // names (an update offer is a panel, never live: decision 036), else
        // the top activity.
        std::string target = s.alert->activityId;
        if (!findActivity(s, target)) {
            const Panel* p = findPanel(s, target);
            if (!p || !p->tapAction.empty() || !hasCard(p->content)) target = s.activities.empty() ? "" : s.activities.front().id;
        }
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
        // close, unless theirs says to stay (Content::keepOpen: Settings, decision 030).
        if (!s.homeOpen && !shownCardKeepsOpen(s) && !s.stayOpen) collapse(s);
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
    // Only critical interrupts Expanded; under a drag, once it ends (decision 061).
    if (postAlert(s, e.alert, now)) {
        if (s.dragging) s.criticalHeld = true;
        else collapse(s);
    }
}
void handle(IslandState& s, const PillClicked&, Ms now) { onClick(s, now); }
void handle(IslandState& s, const ClickedOutside&, Ms)
{
    if (!s.stayOpen) collapse(s); // pinned: it stays over whatever was clicked (decision 046)
}
void handle(IslandState& s, const TabRowPlaced& e, Ms) { s.tabsOnTop = e.top; }
void handle(IslandState& s, const StayOpenToggled&, Ms now)
{
    if (!isExpanded(s)) return;
    s.stayOpen = !s.stayOpen;
    s.lastPointerAt = now; // unpinned: the idle timeout starts over from here
}
// Esc steps back one level (decision 033): a card opened from a Home widget
// goes back to Home; anything else closes.
void handle(IslandState& s, const EscapePressed&, Ms now)
{
    if (s.selection) { // like Explorer: Esc lets go of what's selected first (decision 064)
        s.selection.reset();
        return;
    }
    if (s.cardFromHome && !s.homeOpen && !s.panels.empty()) openHome(s, now);
    else collapse(s);
}
void handle(IslandState& s, const PointerMoved&, Ms now) { s.lastPointerAt = now; }

// A drag over the island (drag.h, decision 061). A drop on a zone opens its
// card, the Shelf's, and leaves it open showing what came in.
void handle(IslandState& s, const DragEntered& e, Ms) { enterDrag(s, e); }
void handle(IslandState& s, const DragZonesChanged& e, Ms) { changeDragZones(s, e); }
void handle(IslandState& s, const DropHovered& e, Ms) { hoverZone(s, e); }
void handle(IslandState& s, const DragLeft&, Ms now) { leaveDrag(s, now); }
void handle(IslandState& s, const Dropped& e, Ms now)
{
    if (auto card = dropOnZone(s, e, now); card && !card->empty()) expand(s, *card, now);
}

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
    if (mode(s) != Mode::Split || s.lift) return;
    if (s.bubbleDrag || isDrag(e.drag)) s.bubbleDrag = e.drag; // small wobbles stay a click
}

void handle(IslandState& s, const BubbleReleased&, Ms now)
{
    bool swap = swapPreview(s);
    s.bubbleDrag.reset();
    if (swap) handle(s, ActivityPromoted{ s.activities[1].id }, now);
}

void handle(IslandState& s, const BubbleDragCancelled&, Ms) { s.bubbleDrag.reset(); }

// Moving the island along the top edge (lift.h, decision 026).
void handle(IslandState& s, const IslandPressed&, Ms now) { pressIsland(s, now); }
void handle(IslandState& s, const IslandDragged& e, Ms) { dragIsland(s, e.drag); }
void handle(IslandState& s, const IslandReleased&, Ms) { releaseIsland(s); }
void handle(IslandState& s, const ScreenResized& e, Ms) { s.screenWidth = e.width; }
void handle(IslandState& s, const SpotRestored& e, Ms) { s.spot = e.spot; }

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

// The activity shortcut (decision 050): the card of the pill's activity, the
// one a click on the pill opens, from anywhere (Home, another card, an alert).
// Pressed again on that card, it closes, like Open Home's.
void handle(IslandState& s, const ActivityOpened&, Ms now)
{
    if (s.activities.empty()) return;
    const std::string& top = s.activities.front().id;
    if (!s.homeOpen && s.expandedId == top) collapse(s);
    else expand(s, top, now);
}

// A running panel opens like its tab (and becomes the pick); a dormant one shows its card.
void handle(IslandState& s, const TileClicked& e, Ms now)
{
    bool fromHome = s.homeOpen;
    if (findActivity(s, e.id)) {
        handle(s, ActivityPromoted{ e.id }, now);
        expand(s, e.id, now);
        s.cardFromHome = fromHome;
    } else if (const Panel* p = findPanel(s, e.id); p && p->tapAction.empty() && hasCard(p->content)) {
        expand(s, e.id, now); // static widgets have no card; a toggle's tap goes to its provider
        s.cardFromHome = fromHome;
    }
}

// --- Over a card: a text draft, a dialog (card_overlays.h), lists (lists.h) ------

void handle(IslandState& s, const InputFocused& e, Ms now) { focusInput(s, e, now); }
// Editing the field (text_edit.h, decision 043) counts as activity: the card stays open.
Draft* busyDraft(IslandState& s, Ms now)
{
    if (!s.draft) return nullptr;
    s.lastPointerAt = now;
    return &*s.draft;
}
void handle(IslandState& s, const TextTyped& e, Ms now) { if (Draft* d = busyDraft(s, now)) insertAtCaret(*d, e.text); }
void handle(IslandState& s, const TextErased& e, Ms now) { if (Draft* d = busyDraft(s, now)) eraseAtCaret(*d, e.forward, e.word); }
void handle(IslandState& s, const CaretMoved& e, Ms now) { if (Draft* d = busyDraft(s, now)) moveCaret(*d, e.move, e.select); }
void handle(IslandState& s, const CaretPlaced& e, Ms now) { if (Draft* d = busyDraft(s, now)) placeCaret(*d, e.at, e.select); }
void handle(IslandState& s, const WordSelected& e, Ms now) { if (Draft* d = busyDraft(s, now)) selectWordAt(*d, e.at); }
void handle(IslandState& s, const AllSelected&, Ms now) { if (Draft* d = busyDraft(s, now)) selectAll(*d); }
void handle(IslandState& s, const InputSubmitted&, Ms now) { submitText(s, now); }
void handle(IslandState& s, const InputCancelled&, Ms) { dropDraft(s); } // Esc: stop typing; the text waits (decision 050)
void handle(IslandState& s, const DialogShown& e, Ms now) { showDialog(s, e, now); }
void handle(IslandState& s, const DialogClosed&, Ms) { s.dialog.reset(); }
void handle(IslandState& s, const ListScrolled& e, Ms now) { scrollList(s, e, now); } // lists.h, decision 029
void handle(IslandState& s, const ListRevealed& e, Ms) { revealListRow(s, e); }
void handle(IslandState& s, const ListDragStarted& e, Ms now) { startListDrag(s, e, now); }
void handle(IslandState& s, const ListDragMoved& e, Ms now) { moveListDrag(s, e, now); }
void handle(IslandState& s, const ListDragEnded&, Ms) { s.listDrag.reset(); }
void handle(IslandState& s, const FocusTaken&, Ms) { s.focusRequest.reset(); }
void handle(IslandState& s, const ItemSelected& e, Ms now) { selectItem(s, e, now); } // selection.h, decision 064
void handle(IslandState& s, const SelectionSet& e, Ms now) { setSelection(s, e, now); }
void handle(IslandState& s, const SelectionCleared&, Ms) { s.selection.reset(); }

void handle(IslandState& s, const ClockSynced& e, Ms now) { s.clock = WallClock{ e.timeOfDay - now, e.hour24 }; }

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
    return s.bubbleDrag && mode(s) == Mode::Split && bubbleOverPill(s, *s.bubbleDrag);
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
    if (isExpanded(s) && !typing(s) && !s.stayOpen && !s.dragging) consider(s.lastPointerAt + tokens().expandedIdleTimeout);
    if (auto drag = dragDeadline(s)) consider(*drag);
    for (const auto& a : s.alertQueue) consider(a.postedAt + tokens().heldAlertMaxAge + Ms{ 1 });
    if (auto lift = liftDeadline(s)) consider(*lift);
    return next;
}


} // namespace cup
