#pragma once
// A provider's window on Windows (decision 104): one ordinary top-level
// window (taskbar, Alt-Tab, snapping, behind other apps unless kept on top)
// whose whole area is ours: our own title bar and buttons, drawn with
// Composition on the island's Compositor by the island's element painters.
// It draws a WindowScene and reports input; what input means is the app's
// and core's (windowHitTest), never decided here.

#include "core/window_layout.h"
#include "platform/element_visuals.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <functional>
#include <optional>
#include <string>

namespace cup::platform {

struct ProviderWindowCallbacks {
    std::function<void(Vec2 point)> onPointerMoved; // client px
    std::function<void()> onPointerLeft;
    std::function<void(Vec2 point)> onPressDown;
    std::function<void(Vec2 point, bool inside)> onPressRelease; // inside: let go over the window
    std::function<void(Vec2 point, float notches)> onWheel;      // positive: away from the user
    std::function<void(WPARAM key)> onKeyDown;
    std::function<void()> onCloseRequested;  // Alt+F4, the taskbar's Close: the app decides
    std::function<void(Vec2 size)> onResized; // the client area's new size: lay it out again now
    std::function<void()> onMoved;           // the user let go after moving or resizing it
    std::function<bool(Vec2 point)> isCaption; // the title bar's empty part: it drags the window
};

class ProviderWindow {
public:
    ProviderWindow() = default;
    ProviderWindow(const ProviderWindow&) = delete;
    ProviderWindow& operator=(const ProviderWindow&) = delete;
    ~ProviderWindow();

    // A window at `frame` (screen px), hidden until show(). False: logged, nothing made.
    // `resizable` and `maximizable` as the provider asked (WindowSpec).
    bool create(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, Rect frame, Vec2 minSize,
                bool keepOnTop, bool resizable, bool maximizable, ProviderWindowCallbacks callbacks);
    void draw(const WindowScene& scene, Ms now); // its backgrounds and items, at once
    // The first time: `draw` builds its content, and the window shows, with
    // the keys, once the compositor has committed what it drew. Shown before
    // that, a window with no GDI surface showed what was behind it for a frame.
    // `shown`: after it shows (the island raised over a window kept on top).
    void drawThenShow(const std::function<void()>& draw, std::function<void()> shown = {});
    void raise();                                // to the front, with the keys; restored if minimized
    void minimize();
    void refresh(bool coloursOnly); // tokens changed: recoloured, or rebuilt on the next draw()
    // The keyboard's focus ring (decision 033) around `frame` (window coordinates), or none.
    void setFocusRing(std::optional<Rect> frame, float radius);
    // Glass (decision 104): Windows 11's acrylic behind it, the window see-through over it; off, solid.
    void setGlass(bool on);
    bool glass() const { return glass_; }
    // What's under the pointer: an item's key, a sidebar entry, a lit row (page coordinates). ""/nullopt: nothing.
    void setHovered(const std::string& key, std::optional<std::size_t> nav, std::optional<Rect> row, float rowRadius);
    void press(const std::string& key);
    void release(const std::string& key);

    HWND hwnd() const { return hwnd_; }
    Rect frame() const { return normal_; } // where it is on screen, px (its restored place when maximized or minimized)
    Vec2 clientSize() const;

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT message, WPARAM wParam, LPARAM lParam);
    void drawDecor();
    ElementVisuals& visualsFor(const std::string& key);
    void rememberNormal(); // its frame now, unless maximized or minimized
    void paintBackground(); // islandColor, see-through as glass

    HWND hwnd_{};
    ProviderWindowCallbacks callbacks_;
    Vec2 minSize_;
    bool resizable_ = true;
    bool maximizable_ = true;
    bool pressed_ = false;
    bool trackingLeave_ = false;
    Rect normal_;

    wuc::Compositor compositor_{ nullptr };
    GraphicsDevice* device_ = nullptr;
    wuc::Desktop::DesktopWindowTarget target_{ nullptr };
    wuc::ContainerVisual root_{ nullptr };
    wuc::SpriteVisual background_{ nullptr }, paneBackground_{ nullptr };
    wuc::ContainerVisual sidebarDecor_{ nullptr }, sidebarLayer_{ nullptr };
    wuc::ContainerVisual pane_{ nullptr }, scroller_{ nullptr }, pageLayer_{ nullptr }, pageDots_{ nullptr }, rowHover_{ nullptr };
    wuc::ContainerVisual footerLayer_{ nullptr }, chromeLayer_{ nullptr }, focusLayer_{ nullptr };
    bool glass_ = false;
    ElementVisuals sidebar_, page_, footer_, chrome_;

    WindowScene scene_;
    std::optional<std::size_t> hoveredNav_;
    std::optional<Rect> hoveredRow_;
    float hoveredRowRadius_ = 0;
};

} // namespace cup::platform
