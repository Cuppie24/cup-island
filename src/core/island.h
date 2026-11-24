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

// Always available, without being live (decision 009): a card, with its
// icon in the page row (decision 165), or a button there. Starting an activity
// with the panel's id makes it live; ending it makes the panel dormant again.
struct Panel {
    std::string id;         // also the id of the activity it turns into
    Content content;        // `expanded*` for its card
    std::string providerId; // set by the host
    // Its page-row button's tap action (PS §7.2): a click sends this to the
    // provider instead of opening a card. For a quick toggle (theme).
    std::string tapAction;
    // Set: this glyph as a button at one end of the page row (decisions
    // 113, 165), not an icon in its list. A click sends tapAction, or opens
    // its card with none; Content::badge puts a dot on it.
    std::optional<Glyph> rowButton;
    // ...and where in the row: the right corner (Settings), left of the
    // corner's (the theme switch), or the left corner (Search).
    enum class RowSpot { Corner, BesideCorner, LeftCorner };
    RowSpot rowSpot = RowSpot::Corner;
    // Its icon in the page row's list (decision 165), live or dormant. None: a plain one.
    std::optional<Glyph> icon;
    bool operator==(const Panel&) const = default;
};

struct Alert {
    std::string activityId; // empty for a standalone alert (e.g. "Copied")
    Level level = Level::Active;
    Content content;
    std::string coalesceKey; // same key replaces a showing/queued alert
    // Only brings a showing or queued alert with this coalesceKey up to date
    // (its time isn't extended); with none, nothing shows. A track's cover
    // read after its alert went up (decision 056).
    bool refreshOnly = false;
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
    // The caret and the selection's other end (decision 043): UTF-8 byte
    // offsets into `text`. Equal: nothing selected.
    std::size_t caret = 0, anchor = 0;
    std::size_t maxLength = 0; // in characters; typing and pasting stop there (decision 045). 0: no limit
    // A row being written (TextInput::leaveAction, decision 132): left, this is sent with its
    // text instead of keeping it for later (decisions 127, 050). "": an ordinary field.
    std::string leaveAction;
    bool operator==(const Draft&) const = default;
};

// A card's scrolling list (decision 029), by "<activityId>/<list>".
struct ListScroll {
    float offset = 0;                  // px down from the list's top (clamped when shown)
    std::optional<std::size_t> reveal; // a row the provider asked to keep in view (Host::reveal); the wheel clears it
    bool centred = false;              // ...in the middle of the list's window (Host::centre), not just inside it
    bool operator==(const ListScroll&) const = default;
};

// A list row being dragged to a new place (decision 029).
struct ListDrag {
    std::string activityId, list;
    std::size_t row = 0;   // its index in the list
    float grabY = 0, y = 0; // the pointer's y (pill coordinates) when it was picked up, and now
    // The list's scroll when it was picked up: the wheel scrolls the list under
    // a carried row, which stays under the pointer (decision 071).
    float grabOffset = 0;
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
    // Set: a modal over that window of the provider's instead of a card
    // (decision 122): everything under the title bar dimmed until it's answered.
    std::string windowId;
    // Over a window only (decision 142): a text field under the message,
    // starting with this text, all of it selected, with the keys. Confirmed,
    // the confirm action's Action::text is what's in it ("Rename group").
    std::optional<std::string> field;
    std::size_t fieldMaxLength = 0; // ...at most this many characters; 0: no limit
    // Over a window only (decision 165): an icon over the title, saying what
    // kind of question it is at a glance (a gauge: this costs performance).
    std::optional<Glyph> glyph;
    bool operator==(const Dialog&) const = default;
};

// A short note at the bottom of a card, with one action, that goes by
// itself after toastDuration (decision 071): "To-do deleted · Undo". One at a
// time: a new one replaces it; its card closing takes it away.
struct Toast {
    std::string providerId;  // set by the host
    std::string activityId;  // the card it shows on
    std::string text;
    std::string actionLabel; // its button's ("Undo"); empty: no button
    std::string actionId;    // sent to the provider when its button is clicked
    Ms until{};              // set by the host: when it goes
    // Set: over that window of the provider's instead of a card (decision 121),
    // at its page's bottom; its button's action carries the window's id.
    std::string windowId;
    bool operator==(const Toast&) const = default;
};

// A row's context menu, open (decision 086, core/menu.h). One at a time.
struct Menu {
    std::string providerId;
    std::string activityId; // the card it's on
    std::string row;        // the row's key, "<slot>/<row>" (its elements' keys less the item)
    std::vector<MenuItem> items; // as the row had them when it opened: it closes if they change
    // Opened by a right-click: where (card coordinates), its corner goes
    // there. None: by the keys or the row's ›, beside the row.
    std::optional<Vec2> at;
    std::optional<std::size_t> lit; // the item the keys or the pointer are on
    bool operator==(const Menu&) const = default;
};

// What sits over one shown card: its question, its note and a row's menu
// (decisions 020, 071, 086).
struct CardOverlays {
    std::optional<Dialog> dialog;
    std::optional<Toast> toast;
    std::optional<Menu> menu;
    bool operator==(const CardOverlays&) const = default;
};

// --- Events -------------------------------------------------------------
struct ActivityStarted { Activity activity; };           // existing id = replace
struct ActivityUpdated { std::string id; Content content; };
struct ActivityEnded { std::string id; };
struct AlertPosted { Alert alert; };
struct PillClicked {};
// A click in the top band of an expanded island (Scene::collapseZone): it always
// closes, a card that keeps open or a pinned island too (decision 108).
struct TopClicked {};
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
struct ActivitySwitched { std::string id; }; // a page-row icon: a live activity becomes the pick and shows its card (decision 007); a dormant panel's card opens (decision 165)
struct BubbleDragged { Vec2 drag; };    // pressed bubble moved: offset from its home (counts once past dragStartDistance)
struct BubbleReleased {};               // drag let go: over the pill = swap (promote the bubble's activity)
struct BubbleDragCancelled {};          // drag interrupted (capture lost): no swap
struct IslandPressed {};                // mouse-down on the pill (not the bubble): held still, it lifts (decision 026)
struct IslandDragged { Vec2 drag; };    // that press moved: offset from where it went down
struct IslandReleased {};               // that press ended (let go, or capture lost): lifted, it lands there
struct ScreenResized { float width; };  // the island's monitor is this wide, in render-layer pixels
// Lifted, the pointer crossed onto another monitor: the island is on that one
// now, this wide, under the pointer at this x on it (decision 178).
struct IslandCarried { float screenWidth; float pointerX; };
struct SpotRestored { Spot spot; };     // its saved place on this monitor, or Spot{} to centre it (decision 026)
struct PanelPublished { Panel panel; }; // existing id = replace, keeping its place in the page row
struct PanelRemoved { std::string id; };
struct IslandOpened {};                 // its shortcut, or the app launched again: the default page's card, if nothing is open (decision 170)
struct ActivityOpened {};               // its shortcut (decision 050): the pill's activity's card; again on it, closes
struct CardCloseRequested { std::string id; }; // its provider is done with it (Host::closeCard, decision 079): closes if shown
struct CardOpened { std::string id; };  // open this card from anywhere (a page-row button, Search's shortcut): a live activity becomes the pick
struct InputFocused {    // a TextInput was clicked, or an edit button (then `text` is what's edited)
    std::string providerId, activityId, actionId;
    std::string text;      // the draft starts with this
    bool editing = false;  // a Text edited in place: Enter ends it
    std::optional<std::size_t> caret; // where it was clicked (a byte offset into `text`); none: at the end
    std::size_t maxLength = 0;        // the field's (TextInput::maxLength, Text::editMaxLength)
};
// Editing the focused field (decision 043; text_edit.h).
enum class CaretMove { Left, Right, WordLeft, WordRight, Start, End };
struct TextTyped { std::string text; };  // UTF-8: replaces the selection, or goes in at the caret
struct TextErased { bool forward = false, word = false; }; // Backspace / Delete; Ctrl: a word. The selection, if any
struct CaretMoved { CaretMove move = CaretMove::Left; bool select = false; }; // arrows, Home, End; Shift selects
struct CaretPlaced { std::size_t at = 0; bool select = false; };  // a click in the field, a drag (select): a byte offset
struct WordSelected { std::size_t at = 0; };                       // a double-click
struct AllSelected {};                                             // Ctrl+A, a triple-click
struct InputSubmitted {};                // Enter: the app sent the text; the field empties and keeps focus
struct InputCancelled {};                // Esc: stop typing, drop the draft
struct DialogShown { Dialog dialog; };   // a provider asked (Host::ask)
struct DialogClosed {};                  // answered or dismissed: the app sends confirmAction first if confirmed
struct ToastShown { Toast toast; };      // a provider posted one (Host::toast)
struct ToastClosed {};                   // its action was clicked: it goes at once
// A row's menu (decision 086). `at`: a right-click's point (card coordinates);
// none: the keys or the row's ›, beside the row with its first item lit.
struct MenuOpened { std::string activityId, row; std::optional<Vec2> at; };
struct MenuLit { std::optional<std::size_t> item; }; // the keys or the pointer moved onto an item (none: off it)
struct MenuClosed {};                    // chosen (the app sends the item's action next), Esc, a click elsewhere
// The wheel over a list (decision 029): `from` is the offset shown (a reveal
// included), `max` how far it can go (both from the scene).
struct ListScrolled { std::string activityId, list; float from = 0, delta = 0, max = 0; };
// Host::reveal; `centred`: Host::centre. `follow` (Host::follow): only moves a reveal that still
// holds, as it was held (centred or not); once the user has scrolled, nothing.
struct ListRevealed { std::string activityId, list; std::size_t row = 0; bool follow = false; bool centred = false; };
struct ListDragStarted { std::string activityId, list; std::size_t row = 0; float y = 0; float offset = 0; }; // offset: the list's, as shown
struct ListDragMoved { float y = 0; };
struct ListDragEnded {};                 // dropped (the app sends the move first) or cancelled
struct StayOpenToggled {};               // Expanded's pin, at the tab row's right end (decision 046)
struct TabRowPlaced { bool top = true; }; // Settings' Tab row: along the card's top or bottom (decision 048)
// Local wall time at `now`, from the platform (the idle clock).
struct ClockSynced {
    Ms timeOfDay;
    bool hour24 = true;
};

using Event = std::variant<ActivityStarted, ActivityUpdated, ActivityEnded, AlertPosted,
                           PillClicked, TopClicked, ClickedOutside, EscapePressed, PointerMoved,
                           DragEntered, DragLeft, Dropped, HiddenChanged, Tick,
                           BubbleClicked, ActivityPromoted, ActivitySwitched,
                           BubbleDragged, BubbleReleased, BubbleDragCancelled,
                           IslandPressed, IslandDragged, IslandReleased, ScreenResized, IslandCarried, SpotRestored,
                           PanelPublished, PanelRemoved, IslandOpened, ActivityOpened, CardCloseRequested, CardOpened, ClockSynced,
                           InputFocused, TextTyped, TextErased, CaretMoved, CaretPlaced, WordSelected, AllSelected,
                           InputSubmitted, InputCancelled,
                           DialogShown, DialogClosed, ToastShown, ToastClosed, MenuOpened, MenuLit, MenuClosed, ListScrolled, ListRevealed, ListDragStarted, ListDragMoved,
                           ListDragEnded, StayOpenToggled, TabRowPlaced>;

// --- State --------------------------------------------------------------
struct WallClock {
    Ms offset{};        // local time of day = now + offset (mod 24 h)
    bool hour24 = true;
};

struct IslandState {
    std::vector<Activity> activities;  // ranked, best first (PS §6.2)
    std::vector<Panel> panels;         // in publish order: the page row's (decision 165)
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
    Ms lastPointerAt{};
    bool dragging = false;             // an accepted drag is over the island
    bool hidden = false;
    std::optional<WallClock> clock;    // unknown until the platform syncs it
    std::optional<Draft> draft;        // a TextInput has focus (decision 016)
    // A field's unsent text while it isn't focused (decision 050), by
    // "<activityId>/<actionId>": it survives the card closing, a tab switch,
    // Esc. The field shows it, and focused again it carries on from it.
    std::map<std::string, std::string> keptText;
    CardOverlays card;                 // over the island's shown card: a question (decision 020), a note at its bottom (decision 071)
    std::map<std::string, ListScroll> scrolls; // lists' scroll, by "<activityId>/<list>" (decision 029)
    std::optional<ListDrag> listDrag;  // a list row being dragged
    // Pinned open (decision 046): Expanded doesn't time out, and a click
    // outside the island or on a card's empty space doesn't close it. Esc
    // and the card going away still do; closing unpins it.
    bool stayOpen = false;
    bool tabsOnTop = true; // the tab row along the card's top (decision 047), or its bottom: the user's pick (decision 048)
    std::uint64_t nextStartOrder = 1;
};

IslandState reduce(IslandState state, const Event& event, Ms now);

Mode mode(const IslandState& state);        // what's visible, with overlay precedence (PS §3.3)
Mode restingMode(const IslandState& state); // Idle / Compact / Split, ignoring overlays

// The bubble is dragged over the pill: letting go now swaps them. The pill
// lightens to say so (decision 007).
bool swapPreview(const IslandState& state);

// A card is open (a drag or Hidden may still cover it).
bool isExpanded(const IslandState& state);

// Lookups by id: nullptr if there's none.
const Activity* findActivity(const IslandState& state, const std::string& id);
const Panel* findPanel(const IslandState& state, const std::string& id);
// A card's content: its activity's, else its dormant panel's.
const Content* cardContent(const IslandState& state, const std::string& id);

// The card Expanded shows (an activity's, or a dormant panel's): whose it is.
// Nothing when nothing is expanded. A change from one is the
// card closing for its provider (Provider::onCardClosed).
struct ShownCard {
    std::string providerId, activityId;
    bool operator==(const ShownCard&) const = default;
};
std::optional<ShownCard> shownCard(const IslandState& state);

// A panel with a card of its own in the page row's list: not a button there, a toggle or a card-less one.
bool isPage(const Panel& panel);
// The card the island opens with when nothing says which (an idle click, its
// shortcut; decision 170): the pill's activity's, else the first page's. "" = none.
std::string defaultPage(const IslandState& state);

// The shown card, if it's a live activity's (not a dormant
// panel's): what Ctrl+W asks to close (decision 034).
std::optional<ShownCard> shownActivity(const IslandState& state);

// What Esc sends to the shown card's provider first: its Content::backAction,
// so a page goes back to its list before anything closes (decision 033). "" = none.
std::string backAction(const IslandState& state);
// The shown card is its field (Content::fieldFirst): Esc closes it at once,
// rather than stopping the typing first (decision 079).
bool escapeCloses(const IslandState& state);

// Whether a press on the island's empty space squashes it (the press
// feedback of a click that collapses it). Not where that click does nothing:
// a card that stays open on it (Content::keepOpen), a pinned island (decision 071).
bool pressSquashes(const IslandState& state);

// Local time of day at `now`, once the platform has synced the clock.
std::optional<Ms> timeOfDay(const IslandState& state, Ms now);

// When a clock showing `timeOfDay` (at `now`) must change next: the next minute.
Ms nextMinute(Ms timeOfDay, Ms now);

// When reduce(Tick) must run next, or nothing if the island is static.
std::optional<Ms> nextDeadline(const IslandState& state);

const char* toString(Mode mode);
const char* toString(Level level);

} // namespace cup
