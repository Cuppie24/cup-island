#pragma once
// Floating windows (decision 081): a card open as its own window, beside the
// island, which keeps showing its pill. Several at once, from several
// providers; one per card. Part of reduce(); split out of island.cpp.
// (Not "windows.h": that name is the Windows SDK's.)

#include "core/island.h"

#include <vector>

namespace cup {

// The card asks to open as a window (Content::window).
bool opensAsWindow(const IslandState& state, const std::string& cardId);

// Opens the card's window, focused, or brings it to the front if it's open.
// False if its provider already has windowsPerProvider open: the island
// shows the card instead, so the click isn't lost.
bool openWindow(IslandState& state, const std::string& cardId);

void closeWindow(IslandState& state, const std::string& cardId);
void focusWindow(IslandState& state, const std::string& cardId); // "" or a card with no window: none has the keys
void resizeWindow(IslandState& state, const std::string& cardId, Vec2 size); // never below windowMinSize

// After every event: a window whose card is gone closes, its toast goes by
// itself, and focus on a closed window is dropped.
void settleWindows(IslandState& state, Ms now);

FloatingWindow* windowFor(IslandState& state, const std::string& cardId);
const FloatingWindow* windowFor(const IslandState& state, const std::string& cardId);
const FloatingWindow* focusedWindow(const IslandState& state); // nullptr: the island has the keys

// Every card in front of the user: the island's, then each window's. A card
// leaving this list is it closing for its provider (Provider::onCardClosed).
std::vector<ShownCard> shownCards(const IslandState& state);

// Between two shownCards() lists: the cards that closed and the ones that
// came, each told to its provider once (onCardClosed, onCardShown).
struct CardChanges {
    std::vector<ShownCard> closed, shown;
};
CardChanges cardChanges(const std::vector<ShownCard>& before, const std::vector<ShownCard>& after);

// The question over the card that has the keys: what Esc and the dialog's
// buttons answer. nullptr: none.
const Dialog* focusedDialog(const IslandState& state);

// The card is in front of the user, on the island or in a window.
bool cardOnScreen(const IslandState& state, const std::string& cardId);

// A size class's logical size (tokens: windowSmallSize ...).
Vec2 windowSize(WindowSize size);

} // namespace cup
