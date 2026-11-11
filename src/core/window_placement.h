#pragma once
// Where a provider's window opens (decisions 081, 090): where the user left
// it, else by its WindowSpec, always on a screen, and never exactly on
// another one. Pure: the platform passes the screens and the pill, and keeps
// the text below.

#include "core/geometry.h"
#include "core/window_content.h"

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

// Where a window was left: on which screen, and its frame in logical px from
// that screen's work area's top-left. Logical and relative, so a scale
// change or a moved monitor still puts it in the same place.
struct SavedPlace {
    std::string screen;
    Rect frame;
    bool operator==(const SavedPlace&) const = default;
};

// Every window's last place ("<provider>/<window>"), and each provider's
// last one: a window it never opened before opens where its provider's last did.
struct Places {
    std::map<std::string, SavedPlace> byWindow, byProvider;
    bool operator==(const Places&) const = default;
};
void remember(Places& places, const std::string& windowKey, const std::string& providerId, const SavedPlace& place);
std::optional<SavedPlace> recall(const Places& places, const std::string& windowKey, const std::string& providerId);

// A size class's logical size (tokens: windowSmallSize ...).
Vec2 windowSize(WindowSize size);

// The window's frame, physical px: its size class's size, or a resizable
// one's saved size (never under windowMinSize). Saved on a screen that's
// gone, or never saved: by its `start`, on the island's screen. Kept on its
// screen: whole if it fits, else its top-left corner is. Where an `open`
// window's top-left already is, it moves windowCascade right and down.
Rect placeWindow(const WindowSpec& spec, const std::vector<Screen>& screens, const std::string& islandScreen, Rect pill,
                 const std::optional<SavedPlace>& saved, const std::vector<Rect>& open = {});

// The other way: a frame (physical px) on `screen`, to remember.
SavedPlace savedPlace(Rect frame, const Screen& screen);

// As text, a line per place: "window" or "provider", its key, the screen, x, y,
// width, height, tab-separated. Lines that don't read are skipped.
std::string formatPlaces(const Places& places);
Places parsePlaces(const std::string& text);

} // namespace cup
