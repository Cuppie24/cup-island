#pragma once
// Render layer (TR §9.1): one large click-through topmost window that owns all
// the visuals, sized once and never resized.

#include "platform/win.h"

namespace cup::platform {

struct RenderLayer {
    HWND hwnd = nullptr;
    int left = 0; // screen x; the layer always sits at y = 0
    wuc::Compositor compositor{ nullptr };
    wuc::Desktop::DesktopWindowTarget target{ nullptr };
    wuc::ContainerVisual root{ nullptr };
};

// Windows.UI.Composition needs a DispatcherQueue on the thread that creates
// the Compositor; this adapts a plain Win32 message-pump thread into one.
winrt::Windows::System::DispatcherQueueController createDispatcherQueueForCurrentThread();

// Creates the window centered at the top of the primary screen, plus the
// compositor and root visual. Returns hwnd == nullptr on failure.
RenderLayer createRenderLayer(HINSTANCE instance, Vec2 canvas);

} // namespace cup::platform
