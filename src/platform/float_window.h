#pragma once
// A floating card's windows (decision 081), a pair like the island's
// (decision 001): a click-through render window a margin bigger than the
// card (room for its shadow and its ✕), with its own Composition target on
// the island's Compositor; and an input window over the card, its region
// the card's rounded shape and the ✕, so clicks beside it reach what's
// under it. Both topmost tool windows: no taskbar button, not in Alt-Tab.
// Turns messages into callbacks and decides nothing, but for moving itself
// while dragged (startMove).

#include "platform/interaction_layer.h"
#include "platform/win.h"

#include <functional>

namespace cup::platform {

// Points are in the render window's coordinates (the visual tree's: the
// card's top-left is at margin()). With capture, a press dragged off the
// card still reports, as negative or past its size.
struct FloatCallbacks {
    std::function<void(Vec2 point)> onPressDown;
    std::function<void(Vec2 point, Release how)> onPressRelease; // not for a press that moved it
    std::function<void(Vec2 point, bool inside)> onPointerMoved;
    std::function<void()> onPointerLeft;
    std::function<void(Vec2 point, float notches)> onWheel;
    std::function<void(WPARAM key)> onKeyDown; // WM_KEYDOWN, and WM_SYSKEYDOWN
    std::function<void(wchar_t character)> onChar;
    std::function<void(bool focused)> onFocusChanged; // it got the keys, or lost them (WM_SETFOCUS, WM_KILLFOCUS)
    std::function<void()> onCloseRequested;           // Alt+F4 while it has the keys
    std::function<void()> onMoved;                    // a drag that moved it ended: card() is where it is now
};

class FloatWindow {
public:
    FloatWindow() = default;
    FloatWindow(const FloatWindow&) = delete; // its windows point back at it
    FloatWindow& operator=(const FloatWindow&) = delete;
    ~FloatWindow() { destroy(); }

    // Over `card` (screen px), its corners `radius`, the ✕ `closeSize` wide
    // centred on its top-right corner. Hidden until show(). False on failure.
    bool create(HINSTANCE instance, wuc::Compositor const& compositor, Rect card, float radius, float closeSize, FloatCallbacks callbacks);
    void destroy();

    wuc::ContainerVisual root() const { return root_; } // the render window's tree
    float margin() const { return margin_; }            // where the card's top-left is in it
    Rect card() const { return card_; }                 // screen px
    HWND input() const { return input_; }

    // Moves and resizes both windows to a card there (screen px), together.
    void setCard(Rect card, float radius);
    // A press on its empty space: the card follows the pointer until let go,
    // then onMoved. Called from onPressDown, while the press holds capture.
    void startMove();
    // Shown in front of the other floating cards (and under the island,
    // which the app puts back on top), with the keys if `focus`.
    void raise(bool focus);
    void show(bool visible);
    void hideInput(); // it's going: drawn a moment longer, but no clicks or keys now
    void setTextCursor(bool text); // over a text field: the I-beam (decision 043)

    // The window procedure's way back in (GWLP_USERDATA).
    LRESULT handle(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    Vec2 pointFrom(LPARAM lParam) const; // input client coordinates -> render-window coordinates
    void applyRegion();

    HWND render_{}, input_{};
    wuc::Desktop::DesktopWindowTarget target_{ nullptr };
    wuc::ContainerVisual root_{ nullptr };
    FloatCallbacks callbacks_;
    Rect card_;
    float radius_ = 0, closeSize_ = 0, margin_ = 0;
    bool pressed_ = false, trackingLeave_ = false, textCursor_ = false, shown_ = false;
    bool moving_ = false;
    POINT moveFrom_{}; // the cursor where the move started, screen px
    Rect moveCard_;    // ...and the card
};

} // namespace cup::platform
