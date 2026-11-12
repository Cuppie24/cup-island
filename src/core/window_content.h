#pragma once
// What a provider puts in a window of its own (decision 104): pages of
// sections of the island's own rows. Providers describe; the host lays out,
// draws, scrolls, edits text and moves the keys, as it does for cards
// (decisions 004, 014). Pure data.

#include "core/content.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

// How big it first opens: a class, sized by tokens (windowSmallSize ...).
// After that, the size the user left it at (window_placement.h).
enum class WindowSize { Small, Medium, Large };

// Where it first opens. After that, where the user left it.
enum class WindowStart { Centre, NearIsland };

// How a provider asks for a window (Host::openWindow). The id is the
// provider's own name for it ("full", "settings"): one window per id.
struct WindowSpec {
    std::string id; // letters, digits, '-', '_', '.'; unique within the provider
    WindowSize size = WindowSize::Medium;
    WindowStart start = WindowStart::Centre;
    bool resizable = true;  // its frame resizes it, down to windowMinSize; false: one size, and Windows doesn't snap it
    // Windows may make it as big as the screen: a double click on its title
    // bar, dragging it to the screen's top, Win+Up; a double click on its top
    // or bottom edge, as tall as the screen. False: none of them. A provider
    // whose page reads best at one size says so (Settings).
    bool maximizable = true;
    // Above other apps, like the island (a timer kept in view). Off: an
    // ordinary window, behind whatever the user clicks next.
    bool keepOnTop = false;
    // The page to show: on opening, or when it's open already (Settings'
    // "Shortcuts" page from a shortcut's row). "": the first, or the one shown.
    std::string page;
    bool operator==(const WindowSpec&) const = default;
};

// A group of rows under an optional heading, drawn as one rounded panel
// with a line between rows. The rows are the island's (content.h): items,
// switches, menus and fields work as on a card; a whole page scrolls, so
// Row::list's own scrolling isn't used in a window.
struct WindowSection {
    std::string title; // a Heading over its rows; "": none
    std::vector<Row> rows;
    bool operator==(const WindowSection&) const = default;
};

// One page. A window with several has a sidebar listing them; the host
// switches between them (a click, Ctrl+Tab) and tells the provider
// (Action{kWindowPage, text = page id}).
struct WindowPage {
    std::string id;    // stable: names it in actions and in WindowSpec::page
    std::string title; // in the sidebar, and over the page
    // Over the page, under its title, in small print: what it's about. "": none.
    std::string subtitle;
    // A page within this page (Settings' themes, from its Theme row): a back
    // arrow before the title sends this, and so does Esc. "": none.
    std::string backAction;
    Glyph glyph = Glyph::Settings;
    bool badge = false; // a dot in the sidebar: something new on it (decision 038)
    std::vector<WindowSection> sections;
    bool operator==(const WindowPage&) const = default;
};

// The whole window. Sent again whenever anything on it changes, like a
// card's content: the host keeps what the user is doing (the page, the
// scroll, a field's text, the keys) across a new copy.
struct WindowContent {
    std::string title;           // its title bar's, the taskbar's and Alt-Tab's; also its accessible name
    std::optional<Color> accent; // tints live data, as on a card (decision 010)
    std::vector<WindowPage> pages; // one: no sidebar. None: an empty window
    // Pinned under the pages, outside their scrolling: "Done", "Reset". Optional.
    std::vector<Row> footer;
    bool operator==(const WindowContent&) const = default;
};

// The host's action when the user picks a page: Action{kWindowPage, text = its id}.
inline constexpr const char* kWindowPage = "window.page";

// The page to show of `content`: `wanted` if it has it, else its first.
// "": it has none.
std::string pageOrFirst(const WindowContent& content, const std::string& wanted);

} // namespace cup
