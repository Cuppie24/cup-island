#pragma once
// A window's question laid out (decision 122): Host::ask with a windowId puts
// it over that window, as the island's dialog is over a card (decision 020).
// Everything under the title bar is dimmed; a panel in the middle holds the
// title, the message, then Cancel and the confirm button. Pure.

#include "core/island.h"
#include "core/layout.h"
#include "core/slot_layout.h"

#include <vector>

namespace cup {

struct WindowDialogScene {
    Rect scrim;   // window coordinates: dimmed, and nothing under it takes a click
    Rect panel;   // window coordinates
    float radius = 0;
    std::vector<Placed> elements; // window coordinates, keys "dialog/...": its buttons send kDialogConfirm / kDialogCancel
    bool operator==(const WindowDialogScene&) const = default;
};

// `size`: the window's; `top`: its title bar's bottom (the bar stays usable).
WindowDialogScene placeWindowDialog(const Dialog& dialog, Vec2 size, float top, const Source& source, Ms now, const MeasureText& measure);

} // namespace cup
