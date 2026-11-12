#pragma once
// Providers' windows on screen (decision 090): one platform window for each
// window the registry has open, laid out by core (windowLayout) and kept in
// step after every change; where each was left, remembered. What a click,
// a key or the wheel means is core's (windowHitTest) and the registry's.

#include "core/layout.h"
#include "core/open_windows.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <filesystem>
#include <functional>
#include <string>

namespace cup {
class ProviderRegistry;
}

namespace cup::app {

struct WindowHooks {
    HINSTANCE instance{};
    platform::wuc::Compositor compositor{ nullptr }; // the island's: every window's visuals on it
    platform::GraphicsDevice* device = nullptr;      // the island renderer's
    ProviderRegistry* registry = nullptr;            // what's open, and where clicks go
    std::function<Ms()> now;
    MeasureText measure;                             // the island renderer's real font
    std::function<Rect()> pill;                      // the pill on screen (screen px): NearIsland opens under it
    std::function<std::string()> islandScreen;       // the monitor the island is on (Display::id)
    std::filesystem::path placesFile;                // where windows were left (window_placement.h)
    // A key in a window, before anything else: true if it was taken (Settings
    // recording a shortcut's new keys from its own window, decision 032).
    std::function<bool(const WindowKey& window, WPARAM key)> takeKey;
};

void setWindowHooks(WindowHooks hooks);

// The registry's windows changed: the platform windows follow, once the
// current message is handled (a click in a window may close it).
void syncWindows();

// Tokens changed: recoloured (a theme, a cover) or rebuilt (tokens.json, display scale).
void refreshWindows();

// Every window gone at once, places saved: the app is exiting.
void closeAllWindows();

} // namespace cup::app
