#pragma once
// The contract between the island and its data sources (PS §7, Tier 1
// in-process). Built-ins use exactly this: no private back doors (PS §1 rule 3).

#include "core/island.h"
#include "core/open_windows.h"

#include <functional>
#include <optional>
#include <string>

namespace cup {

// What a call to Cup Island's service brought back (Host::callService,
// decision 078). `body` is the service's whole JSON-RPC answer (it may still
// be an error answer); empty unless problem is None.
struct ServiceReply {
    enum class Problem {
        None,
        NotInstalled, // file search was never set up, or was removed
        NotRunning,   // installed, but not answering (Windows restarts it)
        NotAllowed,   // this provider wasn't granted that module
        NoAnswer,     // it went away mid-call, or answered nonsense
    };
    Problem problem = Problem::None;
    std::string body;
};

// A click on a button or a seekable progress bar, routed to its provider.
// From a provider's window (decision 104): `windowId` names it and
// `activityId` is empty; the same items send the same actions there.
struct Action {
    std::string providerId;
    std::string activityId;
    std::string actionId; // the Button's actionId, "seek", or a TextInput's actionId
    float value = 0;      // "seek": target position as a 0..1 fraction
    std::string text;     // a TextInput: what was typed (Enter) or pasted (may hold several lines)
    std::string windowId; // set: from that window of the provider's (decision 104)
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

    // A short note at the bottom of this provider's card, with one action
    // ("Undo"), gone by itself after toastDuration (decision 071). Its
    // button sends Action{toast.actionId}. Default: no toasts (tests' hosts).
    virtual void toast(Toast /*toast*/) {}

    // Scrolls a list on this provider's card (Row::list) just enough to show
    // its `row`th row, now and as long as the user doesn't scroll it (decision 029).
    virtual void reveal(const std::string& activityId, const std::string& list, std::size_t row) = 0;

    // Whatever the provider keeps between runs (decision 028): text under a
    // key, its own (keys are per provider; the format is the provider's).
    // Keys: letters, digits, '-', '_'. "" = nothing saved.
    virtual std::string load(const std::string& key) const = 0;
    virtual void save(const std::string& key, const std::string& text) = 0;
    // Wall time (ms since the epoch), for what must mean the same moment
    // after a restart; now() starts over with each run.
    virtual Ms wallClock() const = 0;
    // Local time minus UTC, now: where the user's day starts (Pomodoro's
    // "today", decision 061). Default: UTC, for hosts that don't know (tests).
    virtual Ms utcOffset() const { return Ms{ 0 }; }

    // Closes this provider's card if it's the one shown and isn't pinned
    // (search, after starting what was picked: decision 079). Default: nothing.
    virtual void closeCard(const std::string& /*activityId*/) {}

    // A window of the provider's own (decision 104): pages of the island's
    // rows, laid out, drawn and kept by the host. Opens it, or, open, gives
    // it this content and brings it to the front. Only just after the user
    // acted on this provider (a click on its card, its card opening): see
    // WindowOpen for what else refuses it. Its actions come to onAction with
    // Action::windowId set. Default: no windows (tests' hosts).
    virtual WindowOpen openWindow(const WindowSpec& /*spec*/, WindowContent /*content*/) { return WindowOpen::NotNow; }
    // New content for an open window, without bringing it forward: the
    // user's page, scroll and typing stay. Not open (closed meanwhile): nothing.
    virtual void updateWindow(const std::string& /*windowId*/, WindowContent /*content*/) {}
    // Closes it. No onWindowClosed follows: the provider knows.
    virtual void closeWindow(const std::string& /*windowId*/) {}

    // Calls Cup Island's service (decision 078): `method` is
    // "<module>.<name>" ("index.query"), `params` a JSON object's text.
    // Allowed only for modules this provider was granted. `done` comes later,
    // on the UI thread; after the provider stopped, never. Providers never
    // start or reach processes themselves. Default: nothing installed (tests).
    virtual void callService(const std::string& /*method*/, const std::string& /*params*/,
                             std::function<void(ServiceReply)> done)
    {
        done({ ServiceReply::Problem::NotInstalled, {} });
    }
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
    // The user closed one of its windows (its ✕, Alt+F4, Esc; decision 104).
    // Not when the provider closed it, or stopped.
    virtual void onWindowClosed(const std::string& /*windowId*/) {}
};

} // namespace cup
