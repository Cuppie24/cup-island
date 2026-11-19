#pragma once
// Providers' own windows (decision 104): which are open, whose, in what
// order, and when a provider may open one. Not the island's state: windows
// aren't ranked, timed out or hidden with it, so they live beside
// IslandState, owned by the ProviderRegistry. Pure.

#include "core/island.h"
#include "core/window_content.h"

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// A window is its provider's: the same id from two providers is two windows.
struct WindowKey {
    std::string providerId, windowId;
    bool operator==(const WindowKey&) const = default;
    auto operator<=>(const WindowKey&) const = default;
};

struct OpenWindow {
    WindowKey key;
    WindowSpec spec;          // as last asked
    WindowContent content;    // as last sent
    std::string page;         // the page shown: one of content.pages, or "" if it has none
    std::uint64_t raised = 0; // changes when it must come to the front: the platform raises it then
    // Its toast (Host::toast with a windowId, decision 121): over its page's
    // bottom until Toast::until, or its button is clicked. One at a time.
    std::optional<Toast> toast;
    // Its question (Host::ask with a windowId, decision 122): a modal over it
    // until answered (its buttons, Esc, Enter). One at a time.
    std::optional<Dialog> dialog;
    bool operator==(const OpenWindow&) const = default;
};

struct Windows {
    std::vector<OpenWindow> open; // oldest first
    // When the user last acted on each provider (a click routed to it, its
    // card shown): opening a window needs one just before (windowOpenGrace).
    std::map<std::string, Ms> actedAt;
    std::uint64_t nextRaise = 1;
    bool operator==(const Windows&) const = default;
};

// What became of a Host::openWindow.
enum class WindowOpen {
    Opened,     // a new window
    Raised,     // it was open: its content replaced, brought to the front
    NotNow,     // the user hadn't just acted on this provider: nothing pops up by itself
    TooMany,    // the provider has windowsPerProvider open already
    BadId,      // empty, or not letters, digits, '-', '_', '.'
};

// The user acted on a provider, now: it may open or raise a window until windowOpenGrace has passed.
void userActed(Windows& windows, const std::string& providerId, Ms now);
// When it did, if that's still within windowOpenGrace of now.
std::optional<Ms> justActed(const Windows& windows, const std::string& providerId, Ms now);

// Opens the window, or replaces an open one's content and raises it. Both
// need a recent user action on the provider; replacing the content of a
// window without raising it is updateWindow, which doesn't.
WindowOpen openWindow(Windows& windows, const std::string& providerId, const WindowSpec& spec, WindowContent content, Ms now);

// New content for an open window, kept where the user is (its page, unless
// that page went). False: it isn't open (the user closed it meanwhile).
bool updateWindow(Windows& windows, const WindowKey& key, WindowContent content);

// The user picked a page (sidebar, Ctrl+Tab). False: no such window or page, or it's shown already.
bool showPage(Windows& windows, const WindowKey& key, const std::string& pageId);

// Closes it: its provider asked, or the user did (✕, Alt+F4, Esc). False: it wasn't open.
bool closeWindow(Windows& windows, const WindowKey& key);

// Every window of a provider that stopped goes with it; the keys it closed.
std::vector<WindowKey> closeProviderWindows(Windows& windows, const std::string& providerId);

const OpenWindow* findWindow(const Windows& windows, const WindowKey& key);

// A window's toast (decision 121): shown until now + toastDuration, in place
// of one it had. False: no such window. New content keeps it.
bool showWindowToast(Windows& windows, const WindowKey& key, Toast toast, Ms now);
// Its button clicked (`actionId` is the toast's): gone. False: no such toast.
bool answerWindowToast(Windows& windows, const WindowKey& key, const std::string& actionId);
// Toasts whose time is up, gone. True if any went.
bool expireWindowToasts(Windows& windows, Ms now);
// When the next one goes, if any is up.
std::optional<Ms> nextWindowToastEnd(const Windows& windows);

// A window's question (decision 122): shown, in place of one it had. False: no such window.
bool showWindowDialog(Windows& windows, const WindowKey& key, Dialog dialog);
// Answered: gone. The action its provider gets: the confirm button's, or none (Cancel, Esc).
// None too if the window has no question.
std::optional<std::string> answerWindowDialog(Windows& windows, const WindowKey& key, bool confirmed);

// Letters, digits, '-', '_', '.', and not empty: safe in a file of places and in element keys.
bool validWindowId(const std::string& id);

} // namespace cup
