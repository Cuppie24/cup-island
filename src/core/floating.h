#pragma once
// Floating cards (decision 081): a card apart from the island, in its look,
// left wherever the user drags it, while the island keeps showing its pill.
// Several at once, from several providers; one per card. Part of reduce();
// split out of island.cpp.

#include "core/island.h"

#include <vector>

namespace cup {

// The card asks to float (Content::floating).
bool opensFloating(const IslandState& state, const std::string& cardId);

// Opens the card floating, with the keys, or brings it to the front of the
// others if it's open. False if it doesn't float.
bool openFloat(IslandState& state, const std::string& cardId);

void closeFloat(IslandState& state, const std::string& cardId); // back into the pill
// What Windows says has the keys: that card's window; "" or a card that isn't floating: none.
void focusFloat(IslandState& state, const std::string& cardId);
void resizeFloat(IslandState& state, const std::string& cardId, Vec2 size); // within its limits (floatLimits)

// After every event: a floating card whose card is gone closes, its toast
// goes by itself, and focus on a closed one is dropped.
void settleFloats(IslandState& state, Ms now);

FloatingCard* floatFor(IslandState& state, const std::string& cardId);
const FloatingCard* floatFor(const IslandState& state, const std::string& cardId);
const FloatingCard* focusedFloat(const IslandState& state); // nullptr: the island has the keys

// The question and note over a card on screen: the island's card's, or a
// floating card's own. nullptr: the card isn't on screen.
CardOverlays* overlaysOf(IslandState& state, const std::string& cardId);
const CardOverlays* overlaysOf(const IslandState& state, const std::string& cardId);

// Every card in front of the user: the island's, then each floating one.
// A card leaving this list is it closing for its provider (Provider::onCardClosed).
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

// The card is in front of the user, on the island or floating.
bool cardOnScreen(const IslandState& state, const std::string& cardId);

// Floating cards are drawn: not while the island is Hidden (a fullscreen
// app), since they float above other apps as it does. They come back with it.
bool floatsShown(const IslandState& state);

// How small and how big the card may be: its FloatSpec's limits, else
// floatMinSize and floatMaxHeight (as wide as it likes).
struct FloatLimits {
    Vec2 least, most;
};
FloatLimits floatLimits(const Content* content);
Vec2 clampSize(Vec2 size, const FloatLimits& limits);

// A width class's logical width (tokens: floatNarrowWidth ...).
float floatWidth(FloatWidth width);

} // namespace cup
