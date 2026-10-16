#pragma once
// The Shelf's widget and card (decision 061). Pure: built from its items.

#include "core/island.h"
#include "providers/shelf/shelf_items.h"

namespace cup::shelf {

inline constexpr const char* kPanelId = "shelf"; // also the provider's id

namespace action {
inline constexpr const char* kOpen = "shelf.open.";     // + the item's id
inline constexpr const char* kRemove = "shelf.remove."; // + the item's id: off the shelf; an original is never touched
inline constexpr const char* kKeep = "shelf.keep.";     // + the item's id: a failed copy kept as a link to its source
inline constexpr const char* kClear = "shelf.clear";    // asks first (a dialog)...
inline constexpr const char* kClearConfirmed = "shelf.clear.confirmed";
inline constexpr const char* kList = "shelf.items";     // the card's list (Row::list)
} // namespace action

// The widget: how many things are on the shelf. The card: each one, a link
// or a copy by its icon, with Open and Remove; a copy under way shows its
// progress, a failed one why and "Keep as link"; a link whose original is
// gone says so. Then Clear.
Panel shelfPanel(const Items& items);

// Clear's question: says the originals are kept when there are links.
Dialog clearQuestion(const Items& items);

} // namespace cup::shelf
