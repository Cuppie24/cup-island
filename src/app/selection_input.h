#pragma once
// The app's side of picking things like files in Explorer (decision 064):
// what a click, a box drawn with the mouse, Shift, Ctrl and the keys mean
// for a card's selectable list. The rules are core's (core/selection); this
// reads the pointer and the keys and turns them into events.

#include "core/island.h"
#include "core/layout.h"
#include "core/provider.h"
#include "platform/win.h"

#include <functional>
#include <optional>

namespace cup::app {

struct SelectionHooks {
    std::function<const IslandState&()> state;
    std::function<const Scene&()> scene;
    std::function<void(const Event&, const char* reason)> dispatch;
    std::function<void(const Action&)> act;                // to a provider
    std::function<void(const Placed& name)> rename;        // starts editing a name in place
    std::function<void(std::optional<Rect> box)> showBox;  // the box being drawn, pill coordinates
    std::function<void(std::optional<Ms> delay)> renameTimer; // fires renameDue() once; nullopt cancels
};

void setSelectionHooks(SelectionHooks hooks);

// A press at `point` (pill coordinates): on a thing, it's picked (Ctrl,
// Shift); on the empty part of its list, a box starts. `element` is what
// hitTest found there. True if it was the selection's: nothing else acts on it.
bool selectionPressed(Vec2 point, const Placed* element);
void selectionMoved(Vec2 point);          // a box being drawn follows
bool selectionReleased(Vec2 point, bool inside); // true if the selection took the release
void renameDue();                         // a slow second click's rename, once no double-click came
void cancelRename();                      // a double-click came: it opens instead

// Ctrl+A, Delete, Enter, F2 on a card with a selectable list. True if taken.
bool selectionKey(WPARAM key, bool ctrl);

// The keyboard's focus landed on a thing (arrows): it's picked, Shift extends.
void focusLanded(const Placed& on, bool shift, bool ctrl);

} // namespace cup::app
