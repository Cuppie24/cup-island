#pragma once
// A provider's window laid out (decision 104): its title bar, a sidebar of
// pages, and the shown page's title and groups of rows (core/groups.h,
// decision 090), in the island's own Placed items so the same painters draw
// them. Pure, like layout(): the platform draws a WindowScene and asks
// windowHitTest what a click means.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/open_windows.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

// A page's entry in the sidebar.
struct NavEntry {
    Rect frame;        // window coordinates: lit when picked or under the pointer
    std::string page;  // its WindowPage::id
    bool picked = false;
    bool badge = false;
};

struct WindowScene {
    Vec2 size;                       // the window's client area, px (tokens' scale)
    std::string title;               // WindowContent::title: the title bar's text for Windows, Alt-Tab
    Rect titleBar;                   // drags the window, but not over its buttons
    std::vector<Placed> chrome;      // minimize and close, at the bar's right (window coordinates)
    std::optional<Rect> sidebar;     // several pages: in islandColor; none: no sidebar
    std::vector<Placed> sidebarItems; // the window's title and each page's glyph and name
    std::vector<NavEntry> nav;
    float navRadius = 0;
    Rect pane;                       // where the page shows: clipped to it, scrolled in it (window coordinates)
    std::vector<Placed> page;        // the page's items, page coordinates: the pane's top-left unscrolled
    float scroll = 0;                // how far the page is scrolled, clamped
    float maxScroll = 0;
    std::vector<Placed> footer;      // WindowContent::footer, under the page (window coordinates)
    std::optional<Rect> footerBar;
    std::string backAction;          // the shown page's: Esc sends it (WindowPage::backAction)
    std::string providerId, windowId;
};

// Lays out `window` at `size`, its page scrolled by `scroll` (clamped).
WindowScene windowLayout(const OpenWindow& window, Vec2 size, float scroll, Ms now, const MeasureText& measure = {});

// The host's own buttons in the title bar.
inline constexpr const char* kWindowMinimize = "window.minimize";
inline constexpr const char* kWindowClose = "window.close";

// What's under a point (window coordinates).
enum class WindowPart {
    None,     // nothing that reacts: the page's or sidebar's empty space
    Caption,  // the title bar's empty part: it drags the window
    Minimize,
    Close,
    Page,     // a sidebar entry: `page` says which
    Content,  // an item that acts: `hit`, its action stamped with the window's id
};
struct WindowHit {
    WindowPart part = WindowPart::None;
    std::optional<Hit> hit;  // Content: the action, and the element's key (page or footer)
    std::string page;        // Page
};
WindowHit windowHitTest(const WindowScene& scene, Vec2 point);

// A point in the window to the page's own coordinates (scrolled).
Vec2 toPage(const WindowScene& scene, Vec2 point);

} // namespace cup
