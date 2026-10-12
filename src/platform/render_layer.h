#pragma once
// Render layer (TR §9.1): one large click-through topmost window that owns all
// the visuals: the monitor's width (the island moves along it, decision 026)
// by the canvas's height. Resized when either changes.

#include "platform/win.h"

namespace cup::platform {

struct RenderLayer {
    HWND hwnd = nullptr;
    int left = 0;  // screen position: spanning its monitor,
    int top = 0;   // hanging from that monitor's top edge
    int width = 0; // the monitor's width: core's screen width (ScreenResized)
    wuc::Compositor compositor{ nullptr };
    wuc::Desktop::DesktopWindowTarget target{ nullptr };
    wuc::ContainerVisual root{ nullptr };
};

// Windows.UI.Composition needs a DispatcherQueue on the thread that creates
// the Compositor; this adapts a plain Win32 message-pump thread into one.
winrt::Windows::System::DispatcherQueueController createDispatcherQueueForCurrentThread();

// Creates the window across the top of `monitor` (screen pixels; see
// displays.h), plus the compositor and root visual. hwnd == nullptr on failure.
RenderLayer createRenderLayer(HINSTANCE instance, Vec2 canvas, Rect monitor);

// A new canvas size or another monitor: resized to span its top again.
void resizeRenderLayer(RenderLayer& layer, Vec2 canvas, Rect monitor);

} // namespace cup::platform
