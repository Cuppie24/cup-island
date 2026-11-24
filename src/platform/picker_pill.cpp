#include "platform/picker_pill.h"

#include "platform/element_painting.h"
#include "platform/element_parts.h"
#include "platform/render_layer.h"
#include "platform/text_format.h"

#include "core/tokens.h"

#include <algorithm>
#include <string>
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
constexpr UINT_PTR kLeaveTimer = 1, kNoteTimer = 2;
Rect g_frame;        // where core put it, before a note widens it
bool g_above = false;
std::string g_note;  // on it instead of its glyph, until kNoteTimer fires
// What the surface was last painted for: a dialog being dragged moves the pill many times a second.
struct Painted {
    Vec2 size;
    bool above = false;
    std::string note;
    Color fill, ink, noteInk;
    float flare = -1;
    bool operator==(const Painted&) const = default;
};
Painted g_painted;

// The island's shape (decision 025), small: flat where it meets the dialog,
// its corners there flaring out into the dialog's edge; round at its free
// end. Drawn for the pill hanging (flat side up); standing, it's mirrored.
winrt::com_ptr<ID2D1PathGeometry> outline(Vec2 size, float flare)
{
    float w = size.x, h = size.y;
    float e = std::min(flare, h / 2), r = std::min(h / 2, h - e);
    winrt::com_ptr<ID2D1PathGeometry> path;
    winrt::check_hresult(g_device->d2dFactory()->CreatePathGeometry(path.put()));
    winrt::com_ptr<ID2D1GeometrySink> sink;
    winrt::check_hresult(path->Open(sink.put()));
    auto arc = [&](float x, float y, float radius, D2D1_SWEEP_DIRECTION way) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x, y), D2D1::SizeF(radius, radius), 0, way, D2D1_ARC_SIZE_SMALL));
    };
    sink->BeginFigure(D2D1::Point2F(0, 0), D2D1_FIGURE_BEGIN_FILLED);
    if (e > 0) arc(e, e, e, D2D1_SWEEP_DIRECTION_CLOCKWISE); // the left flare, curving into the body's side
    sink->AddLine(D2D1::Point2F(e, h - r));
    arc(e + r, h, r, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE);
    sink->AddLine(D2D1::Point2F(w - e - r, h));
    arc(w - e, h - r, r, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE);
    sink->AddLine(D2D1::Point2F(w - e, e));
    if (e > 0) arc(w, 0, e, D2D1_SWEEP_DIRECTION_CLOCKWISE); // the right flare
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    winrt::check_hresult(sink->Close());
    return path;
}

void paint(Vec2 size)
{
    const Tokens& t = tokens();
    Painted now{ size, g_above, g_note, t.islandColor, t.iconColor, t.bodyTextColor, t.flareRadius };
    if (now == g_painted) return;
    g_painted = now;
    auto surface = g_device->createSurface(size);
    g_device->draw(surface, [&](ID2D1DeviceContext* dc) {
        if (g_above) dc->SetTransform(D2D1::Matrix3x2F::Scale(1, -1) * D2D1::Matrix3x2F::Translation(0, size.y));
        dc->FillGeometry(outline(size, t.flareRadius).get(), brushOf(dc, t.islandColor).get());
        dc->SetTransform(D2D1::Matrix3x2F::Identity());
        if (g_note.empty()) paintGlyph(dc, g_device->dwrite(), Glyph::Search, t.pickerGlyphSize, size, t.iconColor);
        else paintLabel(dc, g_device->dwrite(), g_note, size, t.bodyTextColor, TextStyle::Body);
    });
    g_visual.Brush(g_compositor.CreateSurfaceBrush(surface));
}

// The window: core's frame, as wide as a note needs, plus a flare either side.
Rect windowFrame()
{
    const Tokens& t = tokens();
    float width = g_frame.width;
    if (!g_note.empty()) width = std::max(width, measureText(g_device->dwrite(), Text{ .value = g_note, .style = TextStyle::Body }) + 2 * t.pickerNotePadding);
    width += 2 * std::min(t.flareRadius, g_frame.height / 2);
    return snapToPixels({ g_frame.x + (g_frame.width - width) / 2, g_frame.y, width, g_frame.height });
}

void place(UINT flags)
{
    Rect at = windowFrame();
    paint({ at.width, at.height });
    // Over the other ordinary windows each time: the dialog's app came forward with it.
    SetWindowPos(g_hwnd, HWND_TOP, static_cast<int>(at.x), static_cast<int>(at.y), static_cast<int>(at.width), static_cast<int>(at.height),
                 SWP_NOACTIVATE | flags);
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
        } else if (wParam == kNoteTimer) { // the note has been read: the glyph again, at the pill's own width
            KillTimer(hwnd, kNoteTimer);
            g_note.clear();
            if (g_stage == Stage::Shown) place(0);
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

void showPickerPill(Rect frame, bool above)
{
    if (!g_hwnd || frame.width < 1 || frame.height < 1) return;
    g_frame = frame;
    g_above = above;
    // It comes by fading in over pickerFade; following the dialog it moves at once, as one piece with it.
    if (g_stage == Stage::Hidden) g_visual.Opacity(0.0f);
    if (g_stage == Stage::Leaving) KillTimer(g_hwnd, kLeaveTimer);
    if (g_stage != Stage::Shown) fadeTo(g_compositor, g_visual, 1.0f, tokens().pickerFade); // from wherever a fade out left it
    g_stage = Stage::Shown;
    place(SWP_SHOWWINDOW);
}

void sayOnPickerPill(const std::string& text)
{
    if (!g_hwnd) return;
    g_note = text;
    SetTimer(g_hwnd, kNoteTimer, static_cast<UINT>(tokens().pickerNote.count()), nullptr);
    if (g_stage == Stage::Shown) place(0);
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
