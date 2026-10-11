#pragma once
// Interaction layer (TR §9.1): a small window that draws nothing and is kept
// exactly over the visible pill, so it receives exactly the pill's clicks.
// Translates raw window messages into callbacks; decides nothing itself.

#include "platform/win.h"

#include <chrono>
#include <functional>
#include <optional>

namespace cup::platform {

enum class TimerId : UINT_PTR { Deadline = 1, TokensReload = 2 };

// Points are in render-layer coordinates (the same as Scene frames). The
// window covers the pill and, in Split, the bubble beside it.
struct InteractionCallbacks {
    std::function<void(Vec2 point)> onPressDown;
    std::function<void(Vec2 point)> onPressMoved; // while pressed, even off the window (captured)
    std::function<void(std::optional<Vec2> point)> onPressRelease; // nullopt: press interrupted (capture lost)
    std::function<void(Vec2 point)> onClick; // released while still on the window
    std::function<void()> onClickOutside; // something else was activated
    std::function<void()> onPointerMoved;
    std::function<void(WPARAM key)> onKeyDown;
    std::function<void(TimerId)> onTimer;
};

// Creates the window over the pill's initial bounds and starts the
// compositor-tick thread. Closing it (DestroyWindow) ends the message loop.
HWND createInteractionLayer(HINSTANCE instance, int renderLeft, Vec2 size, Vec2 offset, InteractionCallbacks callbacks);

// Follow the morph the renderer just started toward these bounds (the pill,
// plus the bubble in Split), frame by frame (decision 001).
void followMorph(Vec2 toSize, Vec2 toOffset);

// One-shot timer: fires onTimer(id) once after `delay`, replacing an earlier
// request with the same id. nullopt cancels.
void setTimer(TimerId id, std::optional<std::chrono::milliseconds> delay);

// Same as setTimer(id, delay), callable from any thread (e.g. the file watcher).
void setTimerFromAnyThread(TimerId id, std::chrono::milliseconds delay);

} // namespace cup::platform
