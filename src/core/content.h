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
                   Battery, Charging, Bluetooth, Sun, Moon, Copy };

enum class TextStyle { Title, Body, Caption, Digits };

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
    bool operator==(const Text&) const = default;
};
struct Image { std::shared_ptr<const ImageData> data; bool operator==(const Image&) const = default; };
struct Icon { Glyph glyph = Glyph::Music; bool operator==(const Icon&) const = default; };
struct Button {
    Glyph glyph = Glyph::Play;
    std::string actionId; // sent back to the provider when clicked
    bool enabled = true;
    std::optional<bool> on; // toggles only (shuffle, repeat): lit with a dot when on, dimmed when off
    bool operator==(const Button&) const = default;
};
// A bar stretches across its row. A ring is a square that fills its slot's
// height (like art), drawn as an arc from the top, clockwise (decision 010).
// Both fill from the Timeline on the compositor: no updates while it runs.
enum class ProgressShape { Bar, Ring };
struct Progress {
    Timeline timeline;
    bool seekable = false; // bars only
    ProgressShape shape = ProgressShape::Bar;
    bool operator==(const Progress&) const = default;
};
// Elapsed "1:23"; `remaining` "-2:05" (media, right-aligned); `countdown`
// "2:05", time left without the "-", centred (a timer).
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

using Item = std::variant<Text, Image, Icon, Button, Progress, TimeLabel, Waveform, Steps>;

enum class Align { Start, Center, End };

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
    bool operator==(const Row&) const = default;
};

Row row(std::initializer_list<Item> items, Align align = Align::Start);

// A slot is a vertical stack of rows, centred vertically in the slot's frame.
using Slot = std::vector<Row>;

// One activity's content for every presentation (PS §5.1). Empty slots are fine.
struct Content {
    std::string label; // accessible name, also used in logs and tests
    std::optional<Color> accent; // none: white (decision 010)
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
    Slot tile; // a panel's glance on Home (decision 009); a running panel's activity can replace it
    bool operator==(const Content&) const = default;
};

// "1:23", or "1:02:03" past an hour.
std::string formatDuration(Ms duration);

// Time of day: "14:05", or "2:05" on a 12-hour clock (no AM/PM: a glance, decision 009).
std::string formatClock(Ms timeOfDay, bool hour24);

// Any expanded slot filled: there's a card to open (a static Home widget may have none).
bool hasCard(const Content& content);

} // namespace cup
