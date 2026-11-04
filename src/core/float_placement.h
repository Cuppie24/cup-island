#pragma once
// Where a floating card opens (decision 081): where the user left it, or by
// its provider's FloatSpec::start, always on a screen, and never exactly on
// another one. Pure: the platform passes the screens and the pill, and keeps
// the text below.

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

// Where a card was left: on which screen, and its frame in logical px from
// that screen's work area's top-left. Logical and relative, so a scale
// change or a moved monitor still puts it in the same place.
struct SavedPlace {
    std::string screen;
    Rect frame;
    bool operator==(const SavedPlace&) const = default;
};

// Every card's last place, and each provider's last one: a card it never
// placed (a new id each time, like a media session's) opens where its
// provider's last did.
struct Places {
    std::map<std::string, SavedPlace> byCard, byProvider;
    bool operator==(const Places&) const = default;
};
void remember(Places& places, const std::string& cardId, const std::string& providerId, const SavedPlace& place);
std::optional<SavedPlace> recall(const Places& places, const std::string& cardId, const std::string& providerId);

// The card's frame, physical px. `size` is its own (logical, from its
// scene); a resizable one takes its saved size instead. Saved on a screen
// that's gone, or never saved: by its `start`, on the island's screen. Kept
// on its screen: whole if it fits, else its top-left corner is. Where an
// `open` card's top-left already is, it moves floatCascade right and down.
Rect placeFloat(const FloatSpec& spec, Vec2 size, const std::vector<Screen>& screens, const std::string& islandScreen,
                Rect pill, const std::optional<SavedPlace>& saved, const std::vector<Rect>& open = {});

// The other way: a frame (physical px) on `screen`, to remember.
SavedPlace savedPlace(Rect frame, const Screen& screen);

// As text, a line per place: "card" or "provider", its id, the screen, x, y,
// width, height, tab-separated. Lines that don't read are skipped.
std::string formatPlaces(const Places& places);
Places parsePlaces(const std::string& text);

} // namespace cup
