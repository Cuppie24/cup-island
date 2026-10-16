#pragma once
// The Shelf's widget and card (decisions 061, 063). Pure: built from its
// items, the view picked, and the thumbnails the app has read so far.

#include "core/island.h"
#include "providers/shelf/shelf_items.h"

#include <map>
#include <memory>

namespace cup::shelf {

inline constexpr const char* kPanelId = "shelf"; // also the provider's id

namespace action {
inline constexpr const char* kOpen = "shelf.open.";     // + the item's id: a double-click, in its app
inline constexpr const char* kReveal = "shelf.reveal."; // + the item's id: shown in Explorer, selected
inline constexpr const char* kRename = "shelf.rename."; // + the item's id: Action::text = the new name (edited in place)
inline constexpr const char* kRemove = "shelf.remove."; // + the item's id: off the shelf; an original is never touched
inline constexpr const char* kKeep = "shelf.keep.";     // + the item's id: a failed copy kept as a link to its source
inline constexpr const char* kView = "shelf.view";      // cards <-> list
inline constexpr const char* kClear = "shelf.clear";    // asks first (a dialog)...
inline constexpr const char* kClearConfirmed = "shelf.clear.confirmed";
inline constexpr const char* kList = "shelf.items";     // the card's list (Row::list): its grid, or its rows
} // namespace action

enum class View { Cards, List };
using Thumbnails = std::map<std::uint64_t, std::shared_ptr<const ImageData>>;

// The widget: how many things are on the shelf. The card (Large): a grid of
// cards (a thumbnail, the name, what it is) or a list of rows, newest first,
// with Show in folder, Keep as link and Remove shown on hover; the name
// renames it; a double-click opens it. Then Clear.
Panel shelfPanel(const Items& items, View view = View::Cards, const Thumbnails& thumbnails = {});

// Clear's question: says the originals are kept when there are links.
Dialog clearQuestion(const Items& items);

} // namespace cup::shelf
