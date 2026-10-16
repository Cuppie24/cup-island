#pragma once
// The contract between the island and its data sources (PS §7, Tier 1
// in-process). Built-ins use exactly this: no private back doors (PS §1 rule 3).

#include "core/island.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

// A click on a button or a seekable progress bar, routed to its provider.
struct Action {
    std::string providerId;
    std::string activityId;
    std::string actionId; // the Button's actionId, "seek", or a TextInput's actionId
    float value = 0;      // "seek": target position as a 0..1 fraction
    std::string text;     // a TextInput: what was typed (Enter) or pasted (may hold several lines)
    bool operator==(const Action&) const = default;
};

// What a provider can do. Call only on the UI thread (the host offers a way
// to get there from background callbacks).
class Host {
public:
    virtual ~Host() = default;
    virtual void startActivity(Activity activity) = 0; // creates, or replaces one with the same id
    virtual void endActivity(const std::string& activityId) = 0;
    virtual void postAlert(Alert alert) = 0;
    virtual Ms now() const = 0; // same clock as Timeline::anchoredAt and expiresAt

    // Always-available entries on Home (decision 009). Start an activity with
    // the panel's id to make it live; end it to make the panel dormant again.
    virtual void publishPanel(Panel panel) = 0; // creates, or replaces one with the same id
    virtual void removePanel(const std::string& panelId) = 0;

    // Call the provider's onWake() once now() reaches `at` (a phase ending,
    // say). One per provider: a new request replaces the old; nullopt cancels.
    virtual void wakeAt(std::optional<Ms> at) = 0;

    // Asks the user over this provider's card, which must be the one shown
    // (decision 020). On confirm the provider gets Action{dialog.confirmAction};
    // Cancel, Esc or the card closing just dismisses it.
    virtual void ask(Dialog dialog) = 0;

    // Scrolls a list on this provider's card (Row::list) just enough to show
    // its `row`th row (a grid's: its cell), now and as long as the user doesn't
    // scroll it (decision 029). `focus`: the keyboard's focus goes to it too, a
    // hint the host takes once (a file just dropped, decision 063).
    virtual void reveal(const std::string& activityId, const std::string& list, std::size_t row, bool focus = false) = 0;

    // Whatever the provider keeps between runs (decision 028): text under a
    // key, its own (keys are per provider; the format is the provider's).
    // Keys: letters, digits, '-', '_'. "" = nothing saved.
    virtual std::string load(const std::string& key) const = 0;
    virtual void save(const std::string& key, const std::string& text) = 0;
    // Wall time (ms since the epoch), for what must mean the same moment
    // after a restart; now() starts over with each run.
    virtual Ms wallClock() const = 0;
};

// Keys never reach a provider (decision 035): the host owns focus and
// navigation. What a key means for a card comes as a hint in its content
// (Content::backAction) or a request (onCloseRequested).
class Provider {
public:
    virtual ~Provider() = default;
    virtual const char* id() const = 0; // reverse-DNS style, also the prefix of its activity ids
    virtual void start(Host& host) = 0;
    virtual void stop() = 0;
    virtual void onAction(const Action& action) = 0;
    virtual void onWake() {} // the time asked for with Host::wakeAt() has come
    // Its card stopped being shown (closed, or Home or another card took its
    // place). A card with pages within it goes back to its first (Settings).
    virtual void onCardClosed(const std::string& /*activityId*/) {}
    // Its card started being shown: what's on it is in front of the user now
    // (Settings counts what's been seen, decision 048).
    virtual void onCardShown(const std::string& /*activityId*/) {}
    // Ctrl+W on its activity's card (decision 034): the user asks to close it.
    // The provider decides: end it (Host::endActivity), ask first (Host::ask),
    // or ignore it (what a media player does: it closes when the music
    // does). Ignoring is the default: nothing ends behind a provider's back.
    virtual void onCloseRequested(const std::string& /*activityId*/) {}
    // What's selected in one of its card's lists changed (Row::selectable,
    // decision 064): the ids, in the list's order; none = nothing selected.
    virtual void onSelectionChanged(const std::string& /*activityId*/, const std::string& /*list*/, const std::vector<std::string>& /*ids*/) {}
    // A drag over the island (decision 061): where it can land for this
    // provider. Asked when it enters and again when its sizes are in, never
    // as it moves, so it must be quick. None (the default): not for it.
    virtual std::vector<DropZone> dropZones(const DragSummary& /*drag*/) { return {}; }
    // Let go over one of its zones: the dropped things, for it alone (PS §9).
    virtual void onDrop(const std::string& /*zoneId*/, const std::vector<DroppedItem>& /*items*/) {}
};

} // namespace cup
