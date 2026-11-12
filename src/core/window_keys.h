#pragma once
// The keyboard in a provider's window (decision 104), on the island's own
// rules (decisions 031, 033): what the keys can land on, where an arrow, Tab,
// Home or End goes, what Enter does, and how far the page scrolls to show
// it. Pure: the app keeps which one has focus and draws its ring.
//
// Stops: the sidebar's pages ("nav:<page id>"), then the page's items
// ("el:<element key>", the island's focusables over the page), then the
// footer's ("foot:<element key>"). The arrows move within the page as on a
// card; Left from the page's left edge goes to the sidebar, Right from the
// sidebar into the page; Up and Down go through the sidebar's pages.

#include "core/keyboard.h"
#include "core/window_layout.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

struct WindowFocus {
    std::string id;
    Rect frame;        // window coordinates (a page's item scrolled as it is now): where its ring goes
    float radius = 0;  // the ring's corners
    Rect pageFrame;    // a page's item: its frame in the page, unscrolled (what scrolling brings into view)
    Vec2 point;        // where Enter clicks, in its own coordinates: the page's, the footer's (window), or the nav entry's middle
    bool inPage = false;
};

// Every stop, in order: the sidebar, the page in reading order, the footer.
std::vector<WindowFocus> windowFocusables(const WindowScene& scene);
std::optional<WindowFocus> findWindowFocus(const WindowScene& scene, const std::string& id);

// Where `arrow` goes from `current`. Nothing focused yet (or it went): the
// first of the page's stops, so the first press shows where focus is; Left
// then is the sidebar's picked page. Nothing that way: nothing.
std::optional<WindowFocus> moveWindowFocus(const WindowScene& scene, const std::string& current, Arrow arrow);

// Tab and Shift+Tab: the next or previous stop, round at the ends.
std::optional<WindowFocus> stepWindowFocus(const WindowScene& scene, const std::string& current, bool backwards);

// Home and End: the page's first or last stop.
std::optional<WindowFocus> pageEndFocus(const WindowScene& scene, bool last);

// Enter and Space on `focus`: what a click there means (a page picked, an
// item's action stamped with the window's id). Nothing for a stop that's gone.
WindowHit activateFocus(const WindowScene& scene, const WindowFocus& focus);

// The scroll that shows a page's stop whole, windowPagePadding.y clear of
// the pane's edges, moving as little as it can; `scene.scroll` if it shows already.
float scrollToShow(const WindowScene& scene, const WindowFocus& focus);

} // namespace cup
