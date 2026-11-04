#pragma once
// The app's floating cards (decision 081): a window pair, a renderer and a
// surface for each card in IslandState::floats, kept in step with it after
// every dispatch. Where each one opens, and where it was left (saved per
// card). What a click on one means is core's; input goes back to the app's
// own handlers with the card's surface.

#include "app/surface.h"
#include "core/island.h"
#include "core/layout.h"
#include "platform/interaction_layer.h"
#include "platform/win.h"

#include <filesystem>
#include <functional>
#include <string>

namespace cup::app {

// What it needs from the app, which owns the state and the loop (main.cpp).
struct FloatHooks {
    HINSTANCE instance{};
    platform::wuc::Compositor compositor{ nullptr };   // the island's: every card's visuals on it
    std::function<void(const Event&, const char* reason)> dispatch;
    MeasureText measure;                                // the island renderer's real font
    std::function<Rect()> pill;                         // the pill on screen (screen px): where a card grows out of
    std::function<std::string()> islandScreen;          // the monitor the island is on (Display::id)
    std::function<float()> scale;                       // logical px to the tokens' (display scale × islandScale)
    std::filesystem::path placesFile;                   // where cards were left, a line each (float_placement.h)
    // The app's input handlers, given the card's surface.
    std::function<void(Surface&, Vec2)> pressDown;
    std::function<void(Surface&, Vec2, platform::Release)> pressRelease;
    std::function<void(Surface&, Vec2, bool inside)> pointerMoved;
    std::function<void(Surface&)> pointerLeft;
    std::function<void(Surface&, Vec2, float notches)> wheel;
    std::function<void(WPARAM key)> keyDown;
    std::function<void(wchar_t character)> character;
};

// Set before the first dispatch.
void setFloatHooks(FloatHooks hooks);

// After every dispatch: lays the cards out from this state, then, once the
// dispatch is over (queued: raising a card dispatches again), opens the
// cards that came (popping in, with the keys), redraws the rest, brings one
// forward when core says, hides them while the island is Hidden, and sends
// back the ones that went (remembering where they were).
void syncFloats(const IslandState& state, Ms now);

Surface* floatSurface(const std::string& cardId); // nullptr: that card isn't floating
void forEachFloat(const std::function<void(Surface&)>& visit);

// A press on a floating card's empty space: it follows the pointer (decision 081).
void startMoving(const Surface& surface);
void setFloatTextCursor(const Surface& surface, bool text); // the I-beam over a field (decision 043)

// Closes every window now, without animation: the app is exiting.
void closeAllFloats();

} // namespace cup::app
