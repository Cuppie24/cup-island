#pragma once
// What a provider puts in a window of its own (decision 104): pages of
// sections of the island's own rows. Providers describe; the host lays out,
// draws, scrolls, edits text and moves the keys, as it does for cards
// (decisions 004, 014). Pure data.

#include "core/content.h"

#include <cstdint>
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
    // No smaller than this by its frame (decision 149), a tokens() size; never
    // below windowMinSize. None: windowMinSize. A page that needs room asks
    // (Pomodoro: one card beside its rail).
    std::optional<Vec2> minSize;
    bool operator==(const WindowSpec&) const = default;
};

// A group of rows under an optional heading, drawn as one rounded panel
// with a line between rows. The rows are the island's (content.h): items,
// switches, menus and fields work as on a card; a whole page scrolls, so
// Row::list's own scrolling isn't used in a window.
struct WindowSection {
    std::string title; // a Heading over its rows; "": none
    std::vector<Row> rows;
    // Stable across updates: its items' keys, so a section removed before it
    // doesn't make the ones after it rebuilt. "": its index.
    std::string id;
    // Its title edited in place, like a to-do (decision 043): a click on it
    // sends Action{titleEdit, text}. "": it's plain text.
    std::string titleEdit;
    std::size_t titleMaxLength = 0; // ...at most this many characters; 0: no limit
    // At its title row's end: first titleItems (a count), then titleTrailing
    // (a delete shown on hover). The title keeps the rest of the row.
    std::vector<Item> titleItems, titleTrailing;
    // As a card (WindowPage::cards): no background, a dashed outline. One that
    // adds a card ("New group"), so it doesn't draw the eye like one (decision 122).
    bool dashed = false;
    // As a card: dragged by a handle at its top, shown while the pointer is on
    // it; let go, Action{moveAction, value = its new index among the page's
    // cards that move} (decision 129). "": it stays where it is.
    std::string moveAction;
    // Rows dragged from another card may be dropped in it: their Row::moveAction
    // sends Action{value = index, text = this card's id} (decision 129).
    bool takesRows = false;
    // As a card: a "…" at its title row's end opens a menu of these (decision
    // 142), under it; an item chosen sends its action from the window.
    std::vector<MenuItem> menu;
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
    bool badgeAccent = false; // ...in the accent, not red: something to act on (an update, decision 152)
    // At the sidebar's foot instead of under the others (a timer's settings, decision 117).
    bool atFoot = false;
    // Its title over the page. Off: a page whose header says what it is (the timer's).
    bool titled = true;
    // Rows across the whole pane, over the title and sections, a line under
    // them: what the page is about (the timer's time and controls, decision 117).
    std::vector<Row> header;
    // The line under the header as a bar filling with this timeline, edge to
    // edge, in the window's accent (the timer's phase, decision 122). None: a plain line.
    std::optional<Timeline> headerProgress;
    // Its sections as cards side by side, at least windowCardMinWidth wide and
    // sharing the row, as many to a row as fit, instead of one column (a board
    // of task groups, decisions 117, 120). Their titles go inside them.
    bool cards = false;
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
    // The sidebar as a rail of the pages' glyphs, windowRailWidth wide, the
    // window's title in its title bar instead (decision 117). Off: names too.
    bool rail = false;
    // Pinned under the pages, outside their scrolling: "Done", "Reset". Optional.
    std::vector<Row> footer;
    // A field to give the keys to, once (decision 134): each time focusRequest
    // changes, the host focuses the field whose action is `focus` (a TextInput's
    // actionId, a Text's editAction: a card's titleEdit), its text selected so
    // what's typed replaces it. A group just made, its name ready to write.
    std::string focus;
    std::uint64_t focusRequest = 0;
    bool operator==(const WindowContent&) const = default;
};

// The host's action when the user picks a page: Action{kWindowPage, text = its id}.
inline constexpr const char* kWindowPage = "window.page";

// The page to show of `content`: `wanted` if it has it, else its first.
// "": it has none.
std::string pageOrFirst(const WindowContent& content, const std::string& wanted);

} // namespace cup
