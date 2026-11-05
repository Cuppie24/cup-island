#pragma once
// A search result's context menu (decision 086): what it offers, by what the
// result is, in the owner's order, each with its shortcut. The host opens it
// (a right-click, Right, the row's ›) and runs its keys. Pure, tested on Linux.

#include "core/content.h"
#include "providers/search/search_provider.h"

#include <cstddef>
#include <string>
#include <vector>

namespace cup {

// Where it is on disk: a file's or folder's path, a desktop program's .exe
// (Launchable::location). Empty for Store apps, Settings pages and Control
// Panel items: nothing to reveal, copy or show properties of.
std::string pathOf(const SearchProvider::Result& result);

// It can be started as administrator: a desktop program (a Store app can't
// be, decision 086), or a file that is a program or script.
bool elevates(const SearchProvider::Result& result);

// The menu for the result at `index` (its actions end in it):
//   Open (Enter, bold) · Run as administrator (Ctrl+Shift+Enter)
//   Reveal in Explorer (Ctrl+Shift+E) · Open in Terminal (Ctrl+Shift+T)
//   Copy (Ctrl+C: files and folders) · Copy path (Ctrl+Shift+C)
//   Remove from history (Shift+Delete) · App settings · Uninstall
//   Properties (Alt+Enter)
// each only where it applies.
std::vector<MenuItem> resultMenu(const SearchProvider::Result& result, std::size_t index);

} // namespace cup
