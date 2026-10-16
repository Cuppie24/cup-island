#pragma once
// Interaction layer (TR §9.1): a small window that draws nothing and is kept
// exactly over the visible pill, so it receives exactly the pill's clicks.
// Translates raw window messages into callbacks; decides nothing itself.

#include "platform/win.h"

#include <chrono>
#include <functional>
#include <optional>

namespace cup::platform {

// The window's class: also how a second launch finds the running island (single_instance.h).
inline constexpr wchar_t kInteractionWindowClass[] = L"CupIslandInputWindow";

enum class TimerId : UINT_PTR { Deadline = 1, TokensReload = 2, Rename = 3 }; // Rename: a slow second click (decision 064)

// How a press ended. Inside/Outside: released over this window or off it.
// Cancelled: capture was taken away mid-press.
enum class Release { Inside, Outside, Cancelled };

// Points are in render-layer coordinates (the same as Scene frames). The
// window covers the pill and, in Split, the bubble beside it. While a press
// is held the window has capture, so moves and the release arrive even off it.
struct InteractionCallbacks {
    std::function<void(Vec2 point)> onPressDown;
    std::function<void(Vec2 point, Release how)> onPressRelease; // a click is Release::Inside
    std::function<void()> onClickOutside; // something else was activated
    std::function<void(Vec2 point, bool inside)> onPointerMoved;
    std::function<void()> onPointerLeft;
    std::function<void(WPARAM key)> onKeyDown; // WM_KEYDOWN, and WM_SYSKEYDOWN (keys pressed with Alt)
    std::function<void(WPARAM id)> onHotkey;   // a system-wide shortcut (WM_HOTKEY, platform/hotkeys.h)
    std::function<void(wchar_t character)> onChar; // typed text (WM_CHAR), for a focused text field
    std::function<void(float scale)> onDpiChanged; // the display's scale changed (1.0 = 100%)
    std::function<void(Vec2 point, float notches)> onWheel; // wheel notches over the pill (fractions from touchpads); positive = down
    std::function<void(TimerId)> onTimer;
    std::function<void()> onShowRequested; // the app was launched again (single_instance.h)
    std::function<void()> onDisplaysChanged; // a monitor was plugged, unplugged, resized or rearranged
};

// Creates the window over the pill's initial bounds and starts the
// compositor-tick thread. Closing it (DestroyWindow) ends the message loop.
HWND createInteractionLayer(HINSTANCE instance, int renderLeft, int renderTop, Vec2 size, Vec2 offset, InteractionCallbacks callbacks);

// The render layer moved (another monitor, or resized for a new display
// scale): points and the window's position are relative to it.
void setRenderOrigin(int renderLeft, int renderTop);

// Follow the morph the renderer just started toward these bounds (the pill,
// plus the bubble in Split), frame by frame (decision 001).
void followMorph(Vec2 toSize, Vec2 toOffset);

// Be over these bounds now, with no morph: where a lifted island was let go,
// so the glide to where it lands is followed from there (decision 026).
void jumpTo(Vec2 size, Vec2 offset);

// The pointer is over a text field (the add field, a to-do being edited):
// the I-beam instead of the arrow (decision 043).
void setTextCursor(bool text);

// One-shot timer: fires onTimer(id) once after `delay`, replacing an earlier
// request with the same id. nullopt cancels.
void setTimer(TimerId id, std::optional<std::chrono::milliseconds> delay);

// Same as setTimer(id, delay), callable from any thread (e.g. the file watcher).
void setTimerFromAnyThread(TimerId id, std::chrono::milliseconds delay);

} // namespace cup::platform
