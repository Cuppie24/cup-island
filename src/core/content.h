#pragma once
// What providers put on the island: a subset of the PS §5 content model
// (decision 004). Providers describe; the host lays out and draws. Pure data.

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cup {

using Ms = std::chrono::milliseconds;

// Built-in icons; the renderer maps them to font glyphs.
enum class Glyph { Play, Pause, Next, Previous, Shuffle, Repeat, RepeatOne, Source, Music, Warning, Timer, Stop, Home,
                   Sun, Moon, Copy, Checked, Unchecked, Delete, Edit, OpenApp, Settings, ChevronLeft, ChevronRight, Download, Centre,
                   Minus, Plus, Pin, Pinned };

// Display: a big number, the card's centre (a timer). Heading: a group's name over its rows (Settings'
// "Dark" and "Light"): small, but heavy and full-strength, so it reads as a divider (decision 037).
enum class TextStyle { Title, Body, Caption, Digits, Display, Heading };

// sRGB plus opacity. As an activity's accent (decision 010) it tints only
// live data: progress fills (bar and ring), the current step, waveform bars.
// Never text, icons or the island itself. Every colour the island draws is
// one of these, in tokens.json.
struct Color {
    std::uint8_t r = 255, g = 255, b = 255;
    std::uint8_t a = 255; // opacity
    bool operator==(const Color&) const = default;
};

// Decoded pixels: BGRA, premultiplied alpha, rows of width * 4 bytes.
// Shared and immutable, so state copies are cheap; new pixels = new ImageData.
struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgra;
};

// Playback position, extrapolated by the host so providers don't send
// once-a-second updates (PS §4).
struct Timeline {
    Ms position{};
    Ms duration{};
    Ms anchoredAt{}; // app time at which `position` was true
    float rate = 1.0f;
    bool playing = false;

    Ms positionAt(Ms now) const;   // clamped to [0, duration]
    float fractionAt(Ms now) const; // 0..1, 0 if duration unknown
    bool operator==(const Timeline&) const = default;
};

// --- Items: the leaves a row is made of ------------------------------------
struct Text {
    std::string value;
    TextStyle style = TextStyle::Body;
    bool alignEnd = false; // right-aligned in its frame
    bool marquee = false;  // too long: scrolls ("swims") in a loop instead of ending in "…"
    bool centred = false;  // centred in its frame (wins over alignEnd)
    bool struck = false;   // struck through: a solved to-do
    // Editable in place (decisions 029, 043): a click on it, or a Button with
    // `editsText` and this actionId, turns it into a field holding its text;
    // Enter sends the provider Action{editAction, text}, Esc leaves it as it was.
    std::string editAction;
    std::size_t editMaxLength = 0; // ...and edited, at most this many characters (decision 045). 0: no limit
    // Wraps onto as many lines as it needs instead of ending in "…" (a
    // to-do, decision 045). The only stretching item in its row.
    bool wrap = false;
    // Host-filled: `value` with the spaces where its lines break turned into
    // '\n' (same length, so offsets hold). Empty: one line.
    std::string wrapped;
    bool operator==(const Text&) const = default;
};
struct Image {
    std::shared_ptr<const ImageData> data;
    std::string actionId; // clickable when set: sent back to the provider (media art opens its app)
    bool operator==(const Image&) const = default;
};
struct Icon { Glyph glyph = Glyph::Music; bool operator==(const Icon&) const = default; };
struct Button {
    Glyph glyph = Glyph::Play;
    std::string actionId; // sent back to the provider when clicked
    bool enabled = true;
    std::optional<bool> on; // toggles only (shuffle, repeat): lit with a dot when on, dimmed when off
    std::string label;      // a text button ("Clear all") instead of the glyph; as wide as its text
    bool compact = false;   // smallButtonSize: list rows (a to-do's checkbox), not main controls. Not "small": windows.h #defines it
    bool revealOnHover = false; // hidden until the pointer is on its row: a list item's delete (decision 020)
    bool destructive = false;   // its label in dangerColor: a dialog's "Clear"
    bool editsText = false;     // starts editing the Text whose editAction is this button's actionId (decision 029)
    bool badge = false;         // a dot on it: something new behind it, an update to install (decision 038)
    bool operator==(const Button&) const = default;
};

// A one-line text field the host draws and edits (decisions 016, 043).
// Clicking it focuses it; typing goes to IslandState::draft, with a caret and
// a selection. Enter sends the text to the provider as Action{actionId, text};
// a paste of several lines sends them the same way (the provider splits
// them). Stretches across its row.
struct TextInput {
    std::string actionId;
    std::string placeholder; // shown dim while empty
    // Host-filled from the draft: providers leave these.
    std::string value;
    bool focused = false;
    std::size_t caret = 0, anchor = 0; // byte offsets into `value`; equal: nothing selected
    bool inPlace = false;    // a Text being edited (Text::editAction): drawn like it, so it doesn't jump
    std::string wrapped;     // ...a wrapping one (Text::wrap): `value` broken into lines, like Text::wrapped
    // Set by the provider: at most this many characters (decision 045). 0: no limit.
    std::size_t maxLength = 0;
    bool operator==(const TextInput&) const = default;
};
// A bar stretches across its row. A ring is a square: alone in its row it
// fills the slot's height (like art); next to other items it matches the
// row's height (a ring beside a big timer). Drawn as an arc from the top,
// clockwise (decision 010).
// Both fill from the Timeline on the compositor: no updates while it runs.
enum class ProgressShape { Bar, Ring };
struct Progress {
    Timeline timeline;
    bool seekable = false; // bars only
    ProgressShape shape = ProgressShape::Bar;
    bool operator==(const Progress&) const = default;
};
// Elapsed "1:23"; `remaining` "-2:05" (media, right-aligned); `countdown`
// "2:05", time left without the "-", centred (a timer). Digits labels sit in
// a fixed column (timeLabelWidth); larger styles are as wide as their digits.
struct TimeLabel {
    Timeline timeline;
    bool remaining = false;
    bool countdown = false;
    TextStyle style = TextStyle::Digits; // a countdown that is the main thing shown can be Body or Title
    bool operator==(const TimeLabel&) const = default;
};
struct Waveform { bool active = false; bool operator==(const Waveform&) const = default; };              // "now playing" bars; still dots when inactive
// A row of dots, one per step: `done` filled, the current one (index `done`)
// in the accent, the rest faint. Pomodoro's phases, a multi-step task (decision 010).
struct Steps {
    int count = 0;
    int done = 0;
    bool operator==(const Steps&) const = default;
};

// A colour sample: a circle of `fill` with a dot of `mark` in it, ringed
// faintly so a pale one still shows on a pale island. A theme in Settings'
// list (its island and accent, decision 030).
struct Swatch {
    Color fill;
    Color mark;
    bool operator==(const Swatch&) const = default;
};

using Item = std::variant<Text, Image, Icon, Button, Progress, TimeLabel, Waveform, Steps, TextInput, Swatch>;

enum class Align { Start, Center, End };

// How wide a row lays out (decision 014): the slot, or exactly the span of
// the row above's / below's content, centred with it (a bar as wide as the
// countdown above it, or the dots below it). The host works out the width.
enum class RowSpan { Slot, MatchAbove, MatchBelow };

// How the Expanded card arranges its slots (decision 014). Providers pick a
// preset; the host owns the geometry.
//  Standard: art square left, title centre, corner button, bottom rows (media).
//  Centred:  only `expandedCenter`, full width, rows stacked and centred: a
//            control panel or a timer, nothing to put in an art square.
//  List:     like Centred, with wider sides (listCardInset): a page of
//            settings, rows edge to edge (decision 030).
enum class CardLayout { Standard, Centred, List };

// A horizontal row of items. Text and Progress stretch; everything else has a fixed size.
struct Row {
    std::vector<Item> items;
    Align align = Align::Start;
    bool loose = false; // wider gaps between items (controls: easier to hit, harder to mis-hit)
    // Fixed-size items pinned to the row's start / end edge, e.g. shuffle and
    // repeat around centred media controls. `items` still centre across the
    // full width. Keys number items, then trailing, then leading.
    std::vector<Item> leading;
    std::vector<Item> trailing;
    RowSpan span = RowSpan::Slot;
    // The row's button takes clicks anywhere on the row, not just on itself:
    // a row of Settings. (A to-do's text edits it instead, decision 043.)
    bool clickAnywhere = false;
    // A list (decision 029): rows with the same `list`, back to back, scroll
    // together in a window listVisibleRows rows tall; the host scrolls it
    // with the wheel, smoothly. Host::reveal keeps a row in view.
    std::string list;
    // The list's window in rows, read from its first row; 0: listVisibleRows.
    // A list that is the whole card (Settings' root page) sizes it to the card.
    float visibleRows = 0;
    // Drag to reorder (decision 029): pressed and dragged, the row moves among
    // the list's rows of the same `sortGroup`; dropped in a new place, its
    // provider gets this action with Action::value = the row's new index in the list.
    std::string moveAction;
    int sortGroup = 0;
    // Names the row in element keys instead of its index ("<slot>/<id>/<item>"),
    // so its visuals follow it when the list reorders. No '/'.
    std::string id;
    // A list item (a to-do): listItemPadding above and below, so items stand
    // apart, its content inset by listItemInset, and a faint background across
    // the whole row under the pointer. Its first and last items' click areas
    // reach the row's edges (a to-do's checkbox: easy to hit, decision 043).
    bool listItem = false;
    // Starts a group (a Heading under other rows): sectionGap of space above
    // it, so groups stand apart (decision 037).
    bool section = false;
    // Its content inset like a list item's, without being one (no padding,
    // nothing lit): a heading over list items lines up with their text.
    bool inset = false;
    bool operator==(const Row&) const = default;
};

Row row(std::initializer_list<Item> items, Align align = Align::Start);

// A slot is a vertical stack of rows, centred vertically in the slot's frame.
using Slot = std::vector<Row>;

// One activity's content for every presentation (PS §5.1). Empty slots are fine.
struct Content {
    std::string label; // accessible name, also used in logs and tests
    std::optional<Color> accent; // none: white (decision 010)
    CardLayout card = CardLayout::Standard;
    // Something new needs a look (an update offered, decision 038): a dot on
    // its widget on Home and on its tab, so the way to it is marked.
    bool badge = false;
    // ...and on Home's button too, seen from any card: only for news that
    // matters that much (an update). Other widgets' dots stay on their widgets.
    bool badgeHome = false;
    // Its card stays open when a click lands between its controls: a page of
    // settings, where a near miss shouldn't close it. Esc, a click outside
    // the island or the idle timeout still do (decision 030).
    bool keepOpen = false;
    // A card on one of its own pages (Settings' Theme page): the action Esc
    // sends, as a click on its back arrow would. Empty: Esc goes back to
    // Home or closes, as anywhere else (decision 033).
    std::string backAction;
    Slot compactLeading;
    Slot compactCenter;  // between the two compact squares, e.g. a swimming title
    Slot compactTrailing;
    Slot minimal;
    Slot alertLeading;
    Slot alertCenter;
    Slot expandedLeading;
    Slot expandedCenter;
    Slot expandedTrailing; // a button-sized square in the top-right corner; the centre narrows only when it's used
    Slot expandedBottom;
    // A panel's widget on Home (decisions 009, 019): information only, no
    // buttons (the host leaves them out); a click opens the card. A running
    // panel's activity can replace it.
    Slot tileLeading; // a square at the widget's left, as tall as it: art, an icon, a ring
    Slot tile;        // the rows beside it
    bool operator==(const Content&) const = default;
};

// "1:23", or "1:02:03" past an hour.
std::string formatDuration(Ms duration);

// Time of day: "14:05", or "2:05" on a 12-hour clock (no AM/PM: a glance, decision 009).
std::string formatClock(Ms timeOfDay, bool hour24);

// Any expanded slot filled: there's a card to open (a static Home widget may have none).
bool hasCard(const Content& content);

} // namespace cup
