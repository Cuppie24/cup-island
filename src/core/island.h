#pragma once
// The island's brain. Every input becomes an Event, and reduce() computes the
// next state. Pure: no I/O, no clock reads, no Windows. The caller passes
// `now` and re-runs reduce(Tick) at nextDeadline().
// Rules come from docs/provider-platform-spec.md (PS) §3 and §6.

#include "core/content.h"
#include "core/geometry.h"
#include "core/placement.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
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

// Always available, without being live (decision 009): a tile on Home and a
// card. Starting an activity with the panel's id makes it live; ending it
// makes the panel dormant again.
struct Panel {
    std::string id;         // also the id of the activity it turns into
    Content content;        // `tile` for Home, `expanded*` for its card
    bool wide = false;      // two Home columns instead of one
    std::string providerId; // set by the host
    // The tile's primary tap action (PS §7.2): a click on the tile sends this
    // to the provider instead of opening a card. For a quick toggle (theme).
    std::string tapAction;
    bool operator==(const Panel&) const = default;
};

struct Alert {
    std::string activityId; // empty for a standalone alert (e.g. "Copied")
    Level level = Level::Active;
    Content content;
    std::string coalesceKey; // same key replaces a showing/queued alert
    std::string providerId;  // set by the host
    Ms postedAt{};           // set by reduce()
};

// Text being typed into a TextInput (decision 016). One at a time, on the shown card.
struct Draft {
    std::string providerId;
    std::string activityId; // the card it's on (an activity's, or a dormant panel's)
    std::string actionId;   // the TextInput's, or the edited Text's editAction
    std::string text;
    bool editing = false;   // a Text edited in place (decision 029): Enter ends it, instead of emptying a field
    bool operator==(const Draft&) const = default;
};

// A card's scrolling list (decision 029), by "<activityId>/<list>".
struct ListScroll {
    float offset = 0;                  // px down from the list's top (clamped when shown)
    std::optional<std::size_t> reveal; // a row the provider asked to keep in view (Host::reveal); the wheel clears it
    bool operator==(const ListScroll&) const = default;
};

// A list row being dragged to a new place (decision 029).
struct ListDrag {
    std::string activityId, list;
    std::size_t row = 0;   // its index in the list
    float grabY = 0, y = 0; // the pointer's y (pill coordinates) when it was picked up, and now
    bool operator==(const ListDrag&) const = default;
};

// A question a provider asks over its card (decision 020): "Clear all
// tasks?". The host draws it in place of the card's content, with Cancel and
// a confirm button; confirming sends `confirmAction` to the provider.
struct Dialog {
    std::string providerId;    // set by the host
    std::string activityId;    // the card it's asked over (an activity's, or a dormant panel's)
    std::string title;
    std::string message;
    std::string confirmLabel = "OK";
    std::string confirmAction;
    bool destructive = false;  // the confirm button reads as dangerous (red)
    bool operator==(const Dialog&) const = default;
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
struct BubbleClicked {};                // Split: expand the bubble's (2nd-ranked) activity
struct ActivityPromoted { std::string id; }; // user's pick for the pill: drag-swap or Expanded's pin (decision 007)
struct ActivitySwitched { std::string id; }; // Expanded tab: make it the pick and show its card (decision 007)
struct BubbleDragged { Vec2 drag; };    // pressed bubble moved: offset from its home (counts once past dragStartDistance)
struct BubbleReleased {};               // drag let go: over the pill = swap (promote the bubble's activity)
struct BubbleDragCancelled {};          // drag interrupted (capture lost): no swap
struct IslandPressed {};                // mouse-down on the pill (not the bubble): held still, it lifts (decision 026)
struct IslandDragged { Vec2 drag; };    // that press moved: offset from where it went down
struct IslandReleased {};               // that press ended (let go, or capture lost): lifted, it lands there
struct ScreenResized { float width; };  // the island's monitor is this wide, in render-layer pixels
struct SpotRestored { Spot spot; };     // its saved place on this monitor, or Spot{} to centre it (decision 026)
struct PanelPublished { Panel panel; }; // existing id = replace, keeping its place on Home
struct PanelRemoved { std::string id; };
struct HomeOpened {};                   // Home tab (decision 009)
struct TileClicked { std::string id; }; // a Home tile, not on a button: open its card
struct InputFocused {    // a TextInput was clicked, or an edit button (then `text` is what's edited)
    std::string providerId, activityId, actionId;
    std::string text;      // the draft starts with this
    bool editing = false;  // a Text edited in place: Enter ends it
};
struct TextTyped { std::string text; };  // UTF-8, appended to the draft
struct TextErased {};                    // Backspace: the last character
struct InputSubmitted {};                // Enter: the app sent the text; the field empties and keeps focus
struct InputCancelled {};                // Esc: stop typing, drop the draft
struct DialogShown { Dialog dialog; };   // a provider asked (Host::ask)
struct DialogClosed {};                  // answered or dismissed: the app sends confirmAction first if confirmed
// The wheel over a list (decision 029): `from` is the offset shown (a reveal
// included), `max` how far it can go (both from the scene).
struct ListScrolled { std::string activityId, list; float from = 0, delta = 0, max = 0; };
struct ListRevealed { std::string activityId, list; std::size_t row = 0; }; // Host::reveal
struct ListDragStarted { std::string activityId, list; std::size_t row = 0; float y = 0; };
struct ListDragMoved { float y = 0; };
struct ListDragEnded {};                 // dropped (the app sends the move first) or cancelled
// Local wall time at `now`, from the platform (the idle clock).
struct ClockSynced {
    Ms timeOfDay;
    bool hour24 = true;
};

using Event = std::variant<ActivityStarted, ActivityUpdated, ActivityEnded, AlertPosted,
                           PillClicked, ClickedOutside, EscapePressed, PointerMoved,
                           DragEntered, DragLeft, Dropped, HiddenChanged, Tick,
                           BubbleClicked, ActivityPromoted, ActivitySwitched,
                           BubbleDragged, BubbleReleased, BubbleDragCancelled,
                           IslandPressed, IslandDragged, IslandReleased, ScreenResized, SpotRestored,
                           PanelPublished, PanelRemoved, HomeOpened, TileClicked, ClockSynced,
                           InputFocused, TextTyped, TextErased, InputSubmitted, InputCancelled,
                           DialogShown, DialogClosed, ListScrolled, ListRevealed, ListDragStarted, ListDragMoved,
                           ListDragEnded>;

// --- State --------------------------------------------------------------
struct WallClock {
    Ms offset{};        // local time of day = now + offset (mod 24 h)
    bool hour24 = true;
};

struct IslandState {
    std::vector<Activity> activities;  // ranked, best first (PS §6.2)
    std::vector<Panel> panels;         // in publish order: Home's tile order (decision 009)
    std::string pinnedId;              // the user's pick for the pill; cleared when it ends (decision 007)
    std::optional<Vec2> bubbleDrag;    // the Split bubble is being dragged, this far from home
    float screenWidth = 0;             // the island's monitor, render-layer pixels; 0 = not told (canvas width)
    Spot spot;                         // where the island rests along the top edge (decision 026)
    std::optional<Ms> pressedAt;       // the pill is held down and hasn't moved: it lifts at pressedAt + liftHold
    Vec2 pressDrag;                    // how far that press has moved
    std::optional<float> lift;         // lifted: the pill's centre x is this + pressDrag.x (lift.h)
    std::optional<Alert> alert;        // the alert on screen
    Ms alertUntil{};
    std::deque<Alert> alertQueue;      // waiting alerts, oldest first (PS §6.3)
    std::string expandedId;            // Expanded, showing this activity's card, or a dormant panel's
    bool homeOpen = false;             // Expanded, showing Home (expandedId empty)
    Ms lastPointerAt{};
    bool dragging = false;             // an accepted drag is over the island
    bool hidden = false;
    std::optional<WallClock> clock;    // unknown until the platform syncs it
    std::optional<Draft> draft;        // a TextInput has focus (decision 016)
    std::optional<Dialog> dialog;      // a question over the shown card (decision 020)
    std::map<std::string, ListScroll> scrolls; // lists' scroll, by "<activityId>/<list>" (decision 029)
    std::optional<ListDrag> listDrag;  // a list row being dragged
    std::uint64_t nextStartOrder = 1;
};

IslandState reduce(IslandState state, const Event& event, Ms now);

Mode mode(const IslandState& state);        // what's visible, with overlay precedence (PS §3.3)
Mode restingMode(const IslandState& state); // Idle / Compact / Split, ignoring overlays

// The bubble is dragged over the pill: letting go now swaps them. The pill
// lightens to say so (decision 007).
bool swapPreview(const IslandState& state);

// Home or a card is open (a drag or Hidden may still cover it).
bool isExpanded(const IslandState& state);

// Lookups by id: nullptr if there's none.
const Activity* findActivity(const IslandState& state, const std::string& id);
const Panel* findPanel(const IslandState& state, const std::string& id);

// Local time of day at `now`, once the platform has synced the clock.
std::optional<Ms> timeOfDay(const IslandState& state, Ms now);

// When a clock showing `timeOfDay` (at `now`) must change next: the next minute.
Ms nextMinute(Ms timeOfDay, Ms now);

// When reduce(Tick) must run next, or nothing if the island is static.
std::optional<Ms> nextDeadline(const IslandState& state);

const char* toString(Mode mode);
const char* toString(Level level);

} // namespace cup
