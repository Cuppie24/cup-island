#pragma once
// The Shelf's widget and card (decisions 061, 063). Pure: built from its
// items, the view picked, and the thumbnails the app has read so far.

#include "core/island.h"
#include "providers/shelf/shelf_items.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace cup::shelf {

inline constexpr const char* kPanelId = "shelf"; // also the provider's id

namespace action {
inline constexpr const char* kOpen = "shelf.open.";     // + the item's id: a double-click, in its app
inline constexpr const char* kReveal = "shelf.reveal."; // + the item's id: shown in Explorer, selected
inline constexpr const char* kRename = "shelf.rename."; // + the item's id: Action::text = the new name (edited in place)
inline constexpr const char* kRemove = "shelf.remove."; // + the item's id: asks first, then off the shelf; an original is never touched
inline constexpr const char* kRemoveConfirmed = "shelf.remove.confirmed."; // + the item's id
inline constexpr const char* kDeleteSelected = "shelf.deleteSelected"; // the selection (decision 064): asks first...
inline constexpr const char* kDeleteSelectedConfirmed = "shelf.deleteSelected.confirmed";
inline constexpr const char* kAdd = "shelf.add";        // the add card: Windows' file picker
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
// picked like files in Explorer (decision 064), with Show in folder, Keep as
// link and Remove shown on hover; the name renames it; a double-click opens
// it; an add card last. At its foot: Delete N items while some are picked, else Clear.
Panel shelfPanel(const Items& items, View view = View::Cards, const Thumbnails& thumbnails = {}, std::size_t picked = 0);

// The questions before anything goes: Clear, Remove, Delete N items. They say
// when originals are kept.
Dialog clearQuestion(const Items& items);
Dialog removeQuestion(const Entry& item);
Dialog deleteQuestion(const Items& items, const std::vector<std::uint64_t>& ids);

// "item-12" -> 12: a row's or cell's id back to its item's.
std::optional<std::uint64_t> itemOfRow(const std::string& rowId);

} // namespace cup::shelf
