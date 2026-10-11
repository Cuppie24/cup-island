#pragma once
// The island's brain. Every input becomes an Event, and reduce() computes the
// next state. Pure: no I/O, no clock reads, no Windows. The caller passes
// `now` and re-runs reduce(Tick) at nextDeadline().
// Rules come from docs/provider-platform-spec.md (PS) §3 and §6.

#include "core/content.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cup {

using Ms = std::chrono::milliseconds; // time since app start

enum class Level { Passive, Active, TimeSensitive, Critical }; // PS §6.1

// What the island is showing. PS §3.1.
enum class Mode { Idle, Compact, Split, Alert, Expanded, Receptive, Hidden };

struct Activity {
    std::string id;         // unique across providers: prefix with the provider id
    Level level = Level::Active;
    float relevance = 0.5f;
    Content content;
    std::optional<Ms> expiresAt; // ends by itself at this time (PS §4), e.g. paused media
    std::string providerId;      // set by the host; actions route back to it
    std::uint64_t startOrder = 0; // set by reduce(); ranking tiebreak
};

struct Alert {
    std::string activityId; // empty for a standalone alert (e.g. "Copied")
    Level level = Level::Active;
    Content content;
    std::string coalesceKey; // same key replaces a showing/queued alert
    std::string providerId;  // set by the host
    Ms postedAt{};           // set by reduce()
};

// --- Events -------------------------------------------------------------
struct ActivityStarted { Activity activity; };           // existing id = replace
struct ActivityUpdated { std::string id; Content content; };
struct ActivityEnded { std::string id; };
struct AlertPosted { Alert alert; };
struct PillClicked {};
struct ClickedOutside {};
struct EscapePressed {};
struct PointerMoved {};                 // inside the pill; resets Expanded's idle timeout
struct DragEntered { bool accepted; };  // accepted: some provider takes the dragged types
struct DragLeft {};
struct Dropped {};
struct HiddenChanged { bool hidden; };  // fullscreen app, user toggle, locked session
struct Tick {};                         // a deadline from nextDeadline() was reached

using Event = std::variant<ActivityStarted, ActivityUpdated, ActivityEnded, AlertPosted,
                           PillClicked, ClickedOutside, EscapePressed, PointerMoved,
                           DragEntered, DragLeft, Dropped, HiddenChanged, Tick>;

// --- State --------------------------------------------------------------
struct IslandState {
    std::vector<Activity> activities;  // ranked, best first (PS §6.2)
    std::optional<Alert> alert;        // the alert on screen
    Ms alertUntil{};
    std::deque<Alert> alertQueue;      // waiting alerts, oldest first (PS §6.3)
    std::string expandedId;            // non-empty = Expanded, showing this activity
    Ms lastPointerAt{};
    bool dragging = false;             // an accepted drag is over the island
    bool hidden = false;
    std::uint64_t nextStartOrder = 1;
};

IslandState reduce(IslandState state, const Event& event, Ms now);

Mode mode(const IslandState& state);        // what's visible, with overlay precedence (PS §3.3)
Mode restingMode(const IslandState& state); // Idle / Compact / Split, ignoring overlays

// When reduce(Tick) must run next, or nothing if the island is static.
std::optional<Ms> nextDeadline(const IslandState& state);

const char* toString(Mode mode);
const char* toString(Level level);

} // namespace cup
