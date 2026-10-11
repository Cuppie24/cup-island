#pragma once
// The contract between the island and its data sources (PS §7, Tier 1
// in-process). Built-ins use exactly this: no private back doors (PS §1 rule 3).

#include "core/island.h"

#include <string>

namespace cup {

// A click on a button or a seekable progress bar, routed to its provider.
struct Action {
    std::string providerId;
    std::string activityId;
    std::string actionId; // the Button's actionId, or "seek"
    float value = 0;      // "seek": target position as a 0..1 fraction
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
};

class Provider {
public:
    virtual ~Provider() = default;
    virtual const char* id() const = 0; // reverse-DNS style, also the prefix of its activity ids
    virtual void start(Host& host) = 0;
    virtual void stop() = 0;
    virtual void onAction(const Action& action) = 0;
};

} // namespace cup
