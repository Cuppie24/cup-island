#pragma once
// Render layer (TR §9.1): one large click-through topmost window that owns all
// the visuals. Resized only when the canvas changes (display scale, islandScale).

#include "platform/win.h"

namespace cup::platform {

struct RenderLayer {
    HWND hwnd = nullptr;
    int left = 0; // screen position: centred on its monitor,
    int top = 0;  // hanging from that monitor's top edge
    wuc::Compositor compositor{ nullptr };
    wuc::Desktop::DesktopWindowTarget target{ nullptr };
    wuc::ContainerVisual root{ nullptr };
};

// Windows.UI.Composition needs a DispatcherQueue on the thread that creates
// the Compositor; this adapts a plain Win32 message-pump thread into one.
winrt::Windows::System::DispatcherQueueController createDispatcherQueueForCurrentThread();

// Creates the window centred at the top of `monitor` (screen pixels; see
// displays.h), plus the compositor and root visual. hwnd == nullptr on failure.
RenderLayer createRenderLayer(HINSTANCE instance, Vec2 canvas, Rect monitor);

// A new canvas size or another monitor: resized and re-centred at its top.
void resizeRenderLayer(RenderLayer& layer, Vec2 canvas, Rect monitor);

} // namespace cup::platform
