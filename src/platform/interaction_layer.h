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

// Points are in pill coordinates: this window always covers exactly the pill.
struct InteractionCallbacks {
    std::function<void(Vec2 point)> onPressDown;
    std::function<void()> onPressRelease;
    std::function<void(Vec2 point)> onClick; // released while still on the pill
    std::function<void()> onClickOutside; // something else was activated
    std::function<void()> onPointerMoved;
    std::function<void(WPARAM key)> onKeyDown;
    std::function<void(TimerId)> onTimer;
};

// Creates the window over the pill's initial bounds and starts the
// compositor-tick thread. Closing it (DestroyWindow) ends the message loop.
HWND createInteractionLayer(HINSTANCE instance, int renderLeft, Vec2 size, Vec2 offset, InteractionCallbacks callbacks);

// Follow the morph the renderer just started toward these bounds, frame by
// frame (docs/decisions/001-interaction-layer-replays-morph.md).
void followMorph(Vec2 toSize, Vec2 toOffset);

// One-shot timer: fires onTimer(id) once after `delay`, replacing an earlier
// request with the same id. nullopt cancels.
void setTimer(TimerId id, std::optional<std::chrono::milliseconds> delay);

// Same as setTimer(id, delay), callable from any thread (e.g. the file watcher).
void setTimerFromAnyThread(TimerId id, std::chrono::milliseconds delay);

} // namespace cup::platform
