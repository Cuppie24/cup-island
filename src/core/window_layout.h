#pragma once
// A provider's window laid out (decision 104): its title bar, a sidebar of
// pages, and the shown page's title and groups of rows (core/groups.h,
// decision 090), in the island's own Placed items so the same painters draw
// them. Pure, like layout(): the platform draws a WindowScene and asks
// windowHitTest what a click means.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/open_windows.h"
#include "core/window_dialog.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

struct WindowTyping; // window_fields.h

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
    std::vector<Placed> chrome;      // minimize, maximize and close at the bar's right; with a rail, the title at its left (window coordinates)
    std::optional<Rect> sidebar;     // several pages: in islandColor; none: no sidebar
    bool rail = false;               // ...the pages' glyphs alone (WindowContent::rail)
    std::vector<Placed> sidebarItems; // the window's title and each page's glyph and name
    std::vector<NavEntry> nav;
    float navRadius = 0;
    Rect pane;                       // where the page shows: clipped to it, scrolled in it (window coordinates)
    std::vector<Placed> page;        // the page's items, page coordinates: the pane's top-left unscrolled
    std::vector<Vec2> badges;        // dots on the page's buttons (Button::badge, decision 038), page coordinates: where badgeCentres puts them
    std::optional<Rect> headerRule;  // the line under the page's header (WindowPage::header), page coordinates
    // Its toast (decision 121), floating over the page's bottom: pane coordinates, unscrolled. When it goes.
    std::optional<ToastScene> toast;
    std::optional<Ms> toastUntil;
    std::optional<WindowDialogScene> dialog; // its question (decision 122): over everything under the title bar
    Ms now{};                        // when it was laid out: its time labels show this moment
    float scroll = 0;                // how far the page is scrolled, clamped
    float maxScroll = 0;
    std::vector<Placed> footer;      // WindowContent::footer, under the page (window coordinates)
    std::optional<Rect> footerBar;
    std::string backAction;          // the shown page's: Esc sends it (WindowPage::backAction)
    std::string providerId, windowId;
};

// Lays out `window` at `size`, its page scrolled by `scroll` (clamped).
// `typing`: its fields, filled from what's being typed and what was left in
// them (decision 105), as layout() fills a card's. `maximized`: Windows has it
// as big as the screen, so the bar's middle button restores it.
WindowScene windowLayout(const OpenWindow& window, Vec2 size, float scroll, Ms now, const MeasureText& measure = {},
                         const WindowTyping* typing = nullptr, bool maximized = false);

// The host's own buttons in the title bar.
inline constexpr const char* kWindowMinimize = "window.minimize";
inline constexpr const char* kWindowMaximize = "window.maximize"; // maximizable windows (WindowSpec): as big as the screen, or back
inline constexpr const char* kWindowClose = "window.close";

// What's under a point (window coordinates).
enum class WindowPart {
    None,     // nothing that reacts: the page's or sidebar's empty space
    Caption,  // the title bar's empty part: it drags the window
    Minimize,
    Maximize, // maximizes it, or restores it
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

// When the window must be laid out again by itself: a time label on its page
// ticks over (as sceneDeadline), or its toast's time is up. None: nothing timed.
std::optional<Ms> windowDeadline(const WindowScene& scene);

// The page's row under a point (window coordinates): "<slot>/<row>", the
// prefix of its items' keys, or "". It reveals that row's revealOnHover
// buttons (a task's delete, decision 120), as rowAt does on the island.
// Between a row's items too; within a card, only that card's rows.
std::string windowRowAt(const WindowScene& scene, Vec2 point);

} // namespace cup
