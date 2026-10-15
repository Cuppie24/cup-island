// Fullscreen detection rules (TR §9.2).

#include "core/fullscreen.h"
#include "expect.h"

namespace cup::test {

void runFullscreenTests()
{
    const Rect monitor{ 0, 0, 1920, 1080 };
    auto window = [&](Rect bounds) { return ForegroundWindow{ .bounds = bounds, .monitor = monitor, .onIslandMonitor = true }; };

    expect(shouldHideForFullscreen(window(monitor), false), "borderless window exactly covering the monitor hides");
    expect(shouldHideForFullscreen(window({ -1, -1, 1922, 1082 }), false), "slightly larger than the monitor still counts");
    expect(!shouldHideForFullscreen(window({ -8, -8, 1936, 1048 }), false), "maximized to the work area (taskbar visible) stays");
    expect(!shouldHideForFullscreen(window({ 100, 100, 800, 600 }), false), "normal window stays");

    auto autoHideTaskbar = window({ -8, -8, 1936, 1096 });
    autoHideTaskbar.maximizedWithTitleBar = true;
    expect(!shouldHideForFullscreen(autoHideTaskbar, false), "maximized window with auto-hide taskbar stays");

    auto other = window(monitor);
    other.onIslandMonitor = false;
    expect(!shouldHideForFullscreen(other, false), "fullscreen on another monitor doesn't hide this island");

    auto desktop = window(monitor);
    desktop.ignorable = true;
    expect(!shouldHideForFullscreen(desktop, false), "desktop / Task View / the island itself are ignored");

    auto minimized = window(monitor);
    minimized.minimized = true;
    expect(!shouldHideForFullscreen(minimized, false), "minimized fullscreen app doesn't hide");

    // D3D exclusive fullscreen (decision 059): the game in front hides the island, whatever its bounds report.
    expect(shouldHideForFullscreen(window({ 100, 100, 800, 600 }), true), "D3D exclusive fullscreen with an app in front hides");
    expect(!shouldHideForFullscreen(desktop, true), "alt-tabbed to the desktop: Windows still says exclusive, the island shows");
    expect(!shouldHideForFullscreen(minimized, true), "the game minimized: shows");
    expect(!shouldHideForFullscreen(other, true), "exclusive on another monitor: this island shows (TR §9.2.4)");
    expect(!shouldHideForFullscreen(std::nullopt, true), "no foreground window (a game closing): shows");
    expect(!shouldHideForFullscreen(std::nullopt, false), "no foreground window: visible");
}

} // namespace cup::test
