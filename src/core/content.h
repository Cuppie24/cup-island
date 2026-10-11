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
enum class Glyph { Play, Pause, Next, Previous, Shuffle, Repeat, RepeatOne, Source, Music, Warning };

enum class TextStyle { Title, Body, Caption, Digits };

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
struct Progress { Timeline timeline; bool seekable = false; bool operator==(const Progress&) const = default; };
struct TimeLabel { Timeline timeline; bool remaining = false; bool operator==(const TimeLabel&) const = default; }; // "1:23" / "-2:05"
struct Waveform { bool active = false; bool operator==(const Waveform&) const = default; };              // "now playing" bars; still dots when inactive

using Item = std::variant<Text, Image, Icon, Button, Progress, TimeLabel, Waveform>;

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
    Slot compactLeading;
    Slot compactCenter;  // between the two compact squares, e.g. a swimming title
    Slot compactTrailing;
    Slot minimal;
    Slot alertLeading;
    Slot alertCenter;
    Slot expandedLeading;
    Slot expandedCenter;
    Slot expandedBottom;
    bool operator==(const Content&) const = default;
};

// "1:23", or "1:02:03" past an hour.
std::string formatDuration(Ms duration);

} // namespace cup
