#pragma once
// A surface: one thing on screen the user points at and types into, the
// island. Its scene, the renderer drawing it, its interaction window, and
// the pointer on it. Input handlers are told the
// surface their window belongs to; what a click means stays core's (hitTest).

#include "core/layout.h"
#include "core/tabs.h"
#include "platform/pill_renderer.h"
#include "platform/win.h"

#include <optional>
#include <string>

namespace cup::app {

// The pointer over a surface. Feedback is the renderer's; what a click means
// is core's (hitTest). Content reacts to hover, the pill never does (decision 002).
struct Pointer {
    std::optional<Vec2> at;      // over the surface's shape, its own coordinates
    // Hasn't moved since what's under it changed some other way (a card
    // opened, typing changed the results): it lights nothing until it moves
    // (decision 087). A press or the wheel counts as moving.
    bool still = true;
    std::optional<Vec2> lastMove; // the last move's point (the window's): Windows repeats one when windows change under a still pointer
    std::string pressedKey;      // the button under the mouse-down
    std::optional<Placed> scrub; // the progress bar being dragged
    float scrubFraction = 0;
    std::optional<Vec2> bubblePress; // mouse-down on the Split bubble, render-layer coordinates
    bool dragged = false;            // ...and core took it as a drag (BubbleDragged): not a click
    std::optional<std::string> tabPress;  // mouse-down on an Expanded tab: its activity ("" = Home)
    std::optional<std::string> tilePress; // mouse-down on a Home tile, off its buttons: its panel
    std::optional<Vec2> islandPress; // mouse-down on the pill that may lift it (decision 026): where it went down
    std::optional<Placed> listPress; // mouse-down on a list row that can be dragged (decision 029)
    float listPressY = 0;            // ...where, its own coordinates: dragged dragStartDistance, it's picked up
    Vec2 textPress;                  // a press on an editable Text: where, so editing starts with the caret there
    std::string selecting;           // a press in the focused field: dragging selects (decision 043)
    Ms lastClickAt{};                // ...the presses before it, for double- and triple-clicks
    Vec2 lastClickPoint;
    int clicks = 0;
};

struct Surface {
    Scene scene;                             // as last laid out
    platform::PillRenderer* renderer = nullptr;
    HWND input{};                            // its interaction window: clicks, keys, focus
    Pointer pointer;
};

// Pointer callbacks get render-layer points; content works in the shape's own coordinates.
Vec2 toLocal(const Surface& s, Vec2 point);
bool onShape(const Surface& s, Vec2 point);  // on the pill
bool onBubble(const Surface& s, Vec2 point); // on the Split bubble

// The element with this key, in the scene or its floating toast (decision 071).
const Placed* findElement(const Surface& s, const std::string& key);
// Content under a point (its own coordinates), not a tab, not under the toast.
const Placed* contentUnder(const Surface& s, Vec2 point);
const Tab* tabUnder(const Surface& s, Vec2 point); // the Expanded tab there, if any
bool isCurrent(const Surface& s, const Tab& tab);  // the tab of what's shown: clicking it does nothing

} // namespace cup::app
