#include "platform/provider_window.h"

#include "core/tokens.h"
#include "platform/log.h"
#include "platform/render_layer.h"

#include <dwmapi.h>

#include <algorithm>
#include <cmath>
#include <exception>

namespace cup::platform {
namespace {

constexpr wchar_t kClassName[] = L"CupIslandProviderWindow";

// Windows 10 20H1+ and 11 read it; older builds refuse it, harmlessly (decision 015).
constexpr DWORD kImmersiveDarkMode = 20;
// Windows' own backdrop behind a top-level window (DWMWA_SYSTEMBACKDROP_TYPE):
// Windows 11 22H2 and later. Older builds refuse it, and the window stays solid.
constexpr DWORD kSystemBackdropType = 38;
constexpr int kBackdropNone = 1, kBackdropAcrylic = 3; // DWMSBT_NONE, DWMSBT_TRANSIENTWINDOW

// A pale island (a light theme): Windows' light frame and acrylic, else its dark ones.
BOOL lightTheme() { const Color& c = tokens().islandColor; return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b > 128 ? TRUE : FALSE; }

void registerClass(HINSTANCE instance, WNDPROC proc)
{
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{ sizeof wc };
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kClassName;
    registered = RegisterClassExW(&wc) != 0;
}

wuc::SpriteVisual colourRect(wuc::Compositor const& c, Rect r, const Color& color)
{
    auto v = c.CreateSpriteVisual();
    v.Brush(c.CreateColorBrush(toUiColor(color)));
    v.Size({ r.width, r.height });
    v.Offset({ std::round(r.x), std::round(r.y), 0 });
    return v;
}

wuc::ShapeVisual roundedRect(wuc::Compositor const& c, Rect r, float radius, const Color& color)
{
    auto geometry = c.CreateRoundedRectangleGeometry();
    geometry.Size({ r.width, r.height });
    geometry.CornerRadius({ radius, radius });
    auto shape = c.CreateSpriteShape(geometry);
    shape.FillBrush(c.CreateColorBrush(toUiColor(color)));
    auto v = c.CreateShapeVisual();
    v.Size({ r.width, r.height });
    v.Offset({ std::round(r.x), std::round(r.y), 0 });
    v.Shapes().Append(shape);
    return v;
}

wuc::ShapeVisual dot(wuc::Compositor const& c, Vec2 centre, float radius, const Color& color)
{
    auto geometry = c.CreateEllipseGeometry();
    geometry.Radius({ radius, radius });
    geometry.Center({ radius, radius });
    auto shape = c.CreateSpriteShape(geometry);
    shape.FillBrush(c.CreateColorBrush(toUiColor(color)));
    auto v = c.CreateShapeVisual();
    v.Size({ 2 * radius, 2 * radius });
    v.Offset({ centre.x - radius, centre.y - radius, 0 });
    v.Shapes().Append(shape);
    return v;
}

wuc::ContainerVisual layer(wuc::Compositor const& c, wuc::ContainerVisual const& parent)
{
    auto v = c.CreateContainerVisual();
    parent.Children().InsertAtTop(v);
    return v;
}

} // namespace

ProviderWindow::~ProviderWindow()
{
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0); // nothing reaches this object while it goes
        DestroyWindow(hwnd_);
    }
}

bool ProviderWindow::create(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, Rect frame, Vec2 minSize,
                            bool keepOnTop, bool resizable, ProviderWindowCallbacks callbacks)
{
    callbacks_ = std::move(callbacks);
    minSize_ = minSize;
    resizable_ = resizable;
    compositor_ = compositor;
    device_ = &device;
    registerClass(instance, proc);
    // An ordinary window, its whole area ours (WM_NCCALCSIZE): no GDI surface
    // under the Composition content, so nothing white flashes as it opens or resizes.
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    DWORD exStyle = WS_EX_NOREDIRECTIONBITMAP | (keepOnTop ? WS_EX_TOPMOST : 0);
    hwnd_ = CreateWindowExW(exStyle, kClassName, L"", style, static_cast<int>(std::lround(frame.x)), static_cast<int>(std::lround(frame.y)),
                            static_cast<int>(std::lround(frame.width)), static_cast<int>(std::lround(frame.height)), nullptr, nullptr, instance, this);
    if (!hwnd_) {
        logLine("a provider's window failed (0x%08lx)", GetLastError());
        return false;
    }
    BOOL dark = !lightTheme();
    DwmSetWindowAttribute(hwnd_, kImmersiveDarkMode, &dark, sizeof dark);
    MARGINS shadow{ 0, 0, 0, 1 }; // a sliver of frame kept: Windows' shadow and snap animations stay
    DwmExtendFrameIntoClientArea(hwnd_, &shadow);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    normal_ = frame;

    target_ = createWindowTarget(compositor_, hwnd_);
    root_ = compositor_.CreateContainerVisual();
    target_.Root(root_);
    background_ = colourRect(compositor_, {}, tokens().islandColor);
    root_.Children().InsertAtTop(background_);
    paneBackground_ = colourRect(compositor_, {}, tokens().windowPaneColor);
    root_.Children().InsertAtTop(paneBackground_);
    sidebarDecor_ = layer(compositor_, root_);
    sidebarLayer_ = layer(compositor_, root_);
    pane_ = layer(compositor_, root_);
    pane_.Clip(compositor_.CreateInsetClip()); // the page shows only in the pane
    scroller_ = layer(compositor_, pane_);
    pageLayer_ = layer(compositor_, scroller_);
    pageDots_ = layer(compositor_, scroller_); // buttons' dots (decision 038), over them
    rowHover_ = layer(compositor_, scroller_); // over the row's items: a faint tint, like the island's lit rows
    footerLayer_ = layer(compositor_, root_);
    chromeLayer_ = layer(compositor_, root_);
    focusLayer_ = layer(compositor_, root_); // the keyboard's ring, over everything
    background_.Size(toFloat2(clientSize())); // its colour from the start: it can show before its content is drawn
    sidebar_.init(compositor_, device, sidebarLayer_);
    page_.init(compositor_, device, pageLayer_);
    footer_.init(compositor_, device, footerLayer_);
    chrome_.init(compositor_, device, chromeLayer_);
    return true;
}

void ProviderWindow::draw(const WindowScene& scene, Ms now)
{
    scene_ = scene;
    std::wstring title = winrt::to_hstring(scene.title).c_str();
    SetWindowTextW(hwnd_, title.c_str()); // the taskbar's and Alt-Tab's name
    background_.Size(toFloat2(scene.size));
    paneBackground_.Size({ scene.pane.width, scene.size.y - scene.pane.y });
    paneBackground_.Offset({ scene.pane.x, scene.pane.y, 0 });
    pane_.Size({ scene.pane.width, scene.pane.height });
    pane_.Offset({ scene.pane.x, scene.pane.y, 0 });
    scroller_.Offset({ 0, -std::round(scene.scroll), 0 }); // the page moves as a whole: scrolling redraws nothing
    drawDecor();
    sidebar_.apply(scene.sidebarItems, now);
    page_.apply(scene.page, now);
    footer_.apply(scene.footer, now);
    chrome_.apply(scene.chrome, now);
}

// The shapes around the items: the picked page's highlight (and the hovered
// one's), the sidebar's dots, and the lit row. Groups' backgrounds are items
// (GroupFill), drawn with the rest. Rebuilt whole: a handful of shapes.
void ProviderWindow::drawDecor()
{
    const Tokens& t = tokens();
    sidebarDecor_.Children().RemoveAll();
    for (std::size_t i = 0; i < scene_.nav.size(); ++i) {
        const NavEntry& n = scene_.nav[i];
        if (n.picked) sidebarDecor_.Children().InsertAtTop(roundedRect(compositor_, n.frame, scene_.navRadius, t.tabSelectedColor));
        else if (hoveredNav_ == i) sidebarDecor_.Children().InsertAtTop(roundedRect(compositor_, n.frame, scene_.navRadius, t.hoverColor));
        if (n.badge) sidebarDecor_.Children().InsertAtTop(dot(compositor_, { n.frame.x + n.frame.width - 12, n.frame.y + n.frame.height / 2 },
                                                              t.badgeSize / 2, t.badgeColor));
    }
    // A button's dot: a setting with something new inside (decision 048), where the island draws one.
    pageDots_.Children().RemoveAll();
    for (Vec2 c : scene_.badges) pageDots_.Children().InsertAtTop(dot(compositor_, c, t.badgeSize / 2, t.badgeColor));
    // The row under the pointer, lit over its group's background (a grouped row's listRow).
    rowHover_.Children().RemoveAll();
    if (hoveredRow_) rowHover_.Children().InsertAtTop(roundedRect(compositor_, *hoveredRow_, hoveredRowRadius_, t.hoverColor));
}

void ProviderWindow::show()
{
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void ProviderWindow::raise()
{
    if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
}

void ProviderWindow::minimize() { ShowWindow(hwnd_, SW_MINIMIZE); }

void ProviderWindow::setFocusRing(std::optional<Rect> frame, float radius)
{
    focusLayer_.Children().RemoveAll();
    if (!frame) return;
    // Inside what has focus, as on the island (decision 033): half the line each side of a path inset by half its width.
    float w = tokens().focusRingWidth;
    Rect r{ frame->x + w / 2, frame->y + w / 2, std::max(0.0f, frame->width - w), std::max(0.0f, frame->height - w) };
    auto geometry = compositor_.CreateRoundedRectangleGeometry();
    geometry.Size({ r.width, r.height });
    geometry.CornerRadius({ std::max(0.0f, radius - w / 2), std::max(0.0f, radius - w / 2) });
    auto shape = compositor_.CreateSpriteShape(geometry);
    shape.StrokeBrush(compositor_.CreateColorBrush(toUiColor(tokens().focusRingColor)));
    shape.StrokeThickness(w);
    auto ring = compositor_.CreateShapeVisual();
    ring.Size({ r.width + w, r.height + w });
    ring.Offset({ std::round(r.x), std::round(r.y), 0 });
    ring.Shapes().Append(shape);
    focusLayer_.Children().InsertAtTop(ring);
}

// Glass (decision 104): Windows' acrylic behind the whole window, our
// background see-through over it as the island's is. Where Windows refuses
// the backdrop (before 11 22H2), solid, as it was.
void ProviderWindow::setGlass(bool on)
{
    int type = on ? kBackdropAcrylic : kBackdropNone;
    bool took = SUCCEEDED(DwmSetWindowAttribute(hwnd_, kSystemBackdropType, &type, sizeof type));
    if (on && !took) logLine("a provider's window: Windows has no backdrop for it (before Windows 11 22H2): solid");
    glass_ = on && took;
    MARGINS frame = glass_ ? MARGINS{ -1, -1, -1, -1 } : MARGINS{ 0, 0, 0, 1 }; // the backdrop shows only where DWM's frame is
    DwmExtendFrameIntoClientArea(hwnd_, &frame);
    // With DWM's frame over the whole window, Windows draws its own caption
    // buttons in it, under ours, for a window with a system menu: none as
    // glass. Alt+F4 is ours (WM_SYSKEYDOWN); Alt+Space has no menu then.
    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
    LONG_PTR wanted = glass_ ? (style & ~static_cast<LONG_PTR>(WS_SYSMENU)) : (style | WS_SYSMENU);
    if (wanted != style) {
        SetWindowLongPtrW(hwnd_, GWL_STYLE, wanted);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    paintBackground();
}

void ProviderWindow::paintBackground()
{
    const Tokens& t = tokens();
    BOOL dark = !lightTheme();
    DwmSetWindowAttribute(hwnd_, kImmersiveDarkMode, &dark, sizeof dark); // a theme change: the acrylic's light or dark too
    // Over Windows' own acrylic, which evens out what's behind as the island's
    // plain blur doesn't: the plain blur's opacities (glassPlainOpacity...), at the Transparency pick.
    Color fill = glass_ ? islandFill(t, t.islandColor, false) : t.islandColor;
    background_.Brush(compositor_.CreateColorBrush(toUiColor(fill)));
}

void ProviderWindow::refresh(bool coloursOnly)
{
    for (ElementVisuals* v : { &sidebar_, &page_, &footer_, &chrome_ }) v->refresh(coloursOnly);
    paintBackground();
    paneBackground_.Brush(compositor_.CreateColorBrush(toUiColor(tokens().windowPaneColor)));
    drawDecor();
}

ElementVisuals& ProviderWindow::visualsFor(const std::string& key)
{
    if (key.rfind("chrome/", 0) == 0) return chrome_;
    if (key.rfind("footer/", 0) == 0) return footer_;
    if (key.rfind("side/", 0) == 0 || key.rfind("nav", 0) == 0) return sidebar_;
    return page_;
}

void ProviderWindow::setHovered(const std::string& key, std::optional<std::size_t> nav, std::optional<Rect> row, float rowRadius)
{
    for (ElementVisuals* v : { &sidebar_, &page_, &footer_, &chrome_ }) v->setHovered(v == &visualsFor(key) ? key : std::string());
    if (nav != hoveredNav_ || row != hoveredRow_) {
        hoveredNav_ = nav;
        hoveredRow_ = row;
        hoveredRowRadius_ = rowRadius;
        drawDecor();
    }
}

void ProviderWindow::press(const std::string& key) { visualsFor(key).press(key); }
void ProviderWindow::release(const std::string& key) { visualsFor(key).release(key); }

void ProviderWindow::rememberNormal()
{
    if (!hwnd_ || IsZoomed(hwnd_) || IsIconic(hwnd_)) return;
    RECT r{};
    GetWindowRect(hwnd_, &r);
    normal_ = { static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top) };
}

Vec2 ProviderWindow::clientSize() const
{
    RECT r{};
    GetClientRect(hwnd_, &r);
    return { static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top) };
}

} // namespace cup::platform
