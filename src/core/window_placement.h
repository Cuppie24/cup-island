#pragma once
// Where a floating window opens (decision 081): where the user left it, or
// by its provider's WindowSpec::start, always on a screen. Pure: the
// platform passes the screens and the pill, and keeps the text below.

#include "core/content.h"
#include "core/geometry.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cup {

struct Screen {
    std::string key;  // the platform's stable name for the monitor
    Rect workArea;    // physical px: the screen less the taskbar
    float scale = 1;  // its display scale × islandScale: logical px to physical (decision 017)
};

// Where a provider's window was left: on which screen, and its frame in
// logical px from that screen's work area's top-left. Logical and relative,
// so a scale change or a moved monitor still puts it in the same place.
struct SavedPlace {
    std::string screen;
    Rect frame;
    bool operator==(const SavedPlace&) const = default;
};

// The window's frame, physical px. `size` is its size class's (logical);
// a resizable window takes its saved size instead. Saved on a screen that's
// gone: placed as if never saved, on the island's screen. Kept on its
// screen: whole if it fits, else its top-left corner (the title bar) is.
Rect placeWindow(const WindowSpec& spec, Vec2 size, const std::vector<Screen>& screens, const std::string& islandScreen,
                 Rect pill, const std::optional<SavedPlace>& saved);

// The other way: a frame (physical px) on `screen`, to remember.
SavedPlace savedPlace(Rect frame, const Screen& screen);

// The saved places, by provider id, as text: a line per provider (its id,
// the screen, x, y, width, height, tab-separated). Lines that don't read are skipped.
std::string formatPlaces(const std::map<std::string, SavedPlace>& places);
std::map<std::string, SavedPlace> parsePlaces(const std::string& text);

} // namespace cup
