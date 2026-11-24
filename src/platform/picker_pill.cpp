#include "platform/picker_pill.h"

#include "platform/element_painting.h"
#include "platform/element_parts.h"
#include "platform/render_layer.h"
#include "platform/text_format.h"

#include "core/tokens.h"

#include <utility>

namespace cup::platform {
namespace {

constexpr wchar_t kClassName[] = L"CupIslandPickerPill";

HWND g_hwnd{};
GraphicsDevice* g_device = nullptr;
wuc::Compositor g_compositor{ nullptr };
wuc::Desktop::DesktopWindowTarget g_target{ nullptr };
wuc::SpriteVisual g_visual{ nullptr };
std::function<void()> g_onClick;
bool g_pressed = false;
// Shown: on screen. Leaving: fading out, hidden when kLeaveTimer fires. Hidden: its window is.
enum class Stage { Hidden, Shown, Leaving };
Stage g_stage = Stage::Hidden;
constexpr UINT_PTR kLeaveTimer = 1;
// What the surface was last painted for: a dialog being dragged moves the pill many times a second.
Vec2 g_paintedSize;
Color g_paintedFill, g_paintedInk;

// Flat along the top, where it meets the dialog; round at the bottom, like
// the island under the screen's edge. The search glyph in its middle.
void paint(Vec2 size)
{
    if (size == g_paintedSize && tokens().islandColor == g_paintedFill && tokens().iconColor == g_paintedInk) return;
    g_paintedSize = size;
    g_paintedFill = tokens().islandColor;
    g_paintedInk = tokens().iconColor;
    auto surface = g_device->createSurface(size);
    g_device->draw(surface, [&](ID2D1DeviceContext* dc) {
        const Tokens& t = tokens();
        float radius = size.y / 2;
        // Twice as tall, its upper half above the surface: only the bottom corners show.
        dc->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, -size.y, size.x, size.y), radius, radius), brushOf(dc, t.islandColor).get());
        paintGlyph(dc, g_device->dwrite(), Glyph::Search, t.pickerGlyphSize, size, t.iconColor);
    });
    g_visual.Brush(g_compositor.CreateSurfaceBrush(surface));
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE; // the dialog keeps the keys until the card opens
    case WM_LBUTTONDOWN:
        g_pressed = g_stage == Stage::Shown; // on its way out it's no longer there to click
        return 0;
    case WM_TIMER:
        if (wParam == kLeaveTimer) {
            KillTimer(hwnd, kLeaveTimer);
            if (std::exchange(g_stage, Stage::Hidden) == Stage::Leaving) ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (std::exchange(g_pressed, false) && g_onClick) g_onClick();
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

} // namespace

void createPickerPill(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, std::function<void()> onClick)
{
    g_compositor = compositor;
    g_device = &device;
    g_onClick = std::move(onClick);

    WNDCLASSEXW windowClass{ sizeof(windowClass) };
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kClassName;
    RegisterClassExW(&windowClass);

    // As the render layer: Composition owns every pixel, never focused, not
    // in the taskbar. Not click-through: it's there to be clicked. And not
    // topmost: an ordinary window like the dialog it hangs from, so where the
    // dialog goes behind the taskbar, so does the pill.
    g_hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"Cup Island File Pill",
                             WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) return;
    g_target = createWindowTarget(g_compositor, g_hwnd);
    g_visual = g_compositor.CreateSpriteVisual();
    g_visual.RelativeSizeAdjustment({ 1.0f, 1.0f });
    g_target.Root(g_visual);
}

void showPickerPill(Rect frame)
{
    if (!g_hwnd || frame.width < 1 || frame.height < 1) return;
    paint({ frame.width, frame.height });
    // It comes by fading in over pickerFade; following the dialog it moves at once, as one piece with it.
    if (g_stage == Stage::Hidden) g_visual.Opacity(0.0f);
    if (g_stage == Stage::Leaving) KillTimer(g_hwnd, kLeaveTimer);
    if (g_stage != Stage::Shown) fadeTo(g_compositor, g_visual, 1.0f, tokens().pickerFade); // from wherever a fade out left it
    g_stage = Stage::Shown;
    // Over the other ordinary windows each time: the dialog's app came forward with it.
    SetWindowPos(g_hwnd, HWND_TOP, static_cast<int>(frame.x), static_cast<int>(frame.y), static_cast<int>(frame.width),
                 static_cast<int>(frame.height), SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void hidePickerPill()
{
    g_pressed = false;
    if (!g_hwnd || g_stage != Stage::Shown) return;
    // It goes by fading out; its window hides once nothing of it shows.
    g_stage = Stage::Leaving;
    fadeTo(g_compositor, g_visual, 0.0f, tokens().pickerFade);
    SetTimer(g_hwnd, kLeaveTimer, static_cast<UINT>(tokens().pickerFade.count()), nullptr);
}

} // namespace cup::platform
