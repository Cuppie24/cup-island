#pragma once
// The contract between the island and its data sources (PS §7, Tier 1
// in-process). Built-ins use exactly this: no private back doors (PS §1 rule 3).

#include "core/island.h"

#include <optional>
#include <string>

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
};

class Provider {
public:
    virtual ~Provider() = default;
    virtual const char* id() const = 0; // reverse-DNS style, also the prefix of its activity ids
    virtual void start(Host& host) = 0;
    virtual void stop() = 0;
    virtual void onAction(const Action& action) = 0;
    virtual void onWake() {} // the time asked for with Host::wakeAt() has come
};

} // namespace cup
