#include "platform/float_window.h"

#include "platform/glass.h"
#include "platform/log.h"
#include "platform/render_layer.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

namespace cup::platform {
namespace {

constexpr wchar_t kInputClass[] = L"CupIslandFloatInput";
constexpr wchar_t kRenderClass[] = L"CupIslandFloatRender";

LRESULT CALLBACK inputProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
try {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* window = reinterpret_cast<FloatWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return window ? window->handle(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}
catch (winrt::hresult_error const& e) {
    // An exception escaping a window procedure usually ends the process silently.
    logLine("WinRT error in a floating card, message 0x%04x: 0x%08x %ls", message, static_cast<unsigned int>(e.code()), e.message().c_str());
    return 0;
}
catch (std::exception const& e) {
    logLine("Exception in a floating card, message 0x%04x: %s", message, e.what());
    return 0;
}
catch (...) {
    logLine("Unknown exception in a floating card, message 0x%04x", message);
    return 0;
}

void registerClasses(HINSTANCE instance)
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    WNDCLASSEXW input{ sizeof(input) };
    input.lpfnWndProc = inputProc;
    input.hInstance = instance;
    input.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    input.lpszClassName = kInputClass;
    RegisterClassExW(&input);
    WNDCLASSEXW render{ sizeof(render) };
    render.lpfnWndProc = DefWindowProcW; // input never reaches it (WS_EX_TRANSPARENT)
    render.hInstance = instance;
    render.lpszClassName = kRenderClass;
    RegisterClassExW(&render);
}

int px(float v) { return static_cast<int>(std::lround(v)); }

template <typename F, typename... Args>
void call(const F& callback, Args... args)
{
    if (callback) callback(args...);
}

} // namespace

bool FloatWindow::create(HINSTANCE instance, wuc::Compositor const& compositor, Rect card, float radius, FloatCallbacks callbacks)
{
    registerClasses(instance);
    callbacks_ = std::move(callbacks);
    card_ = card;
    radius_ = radius;
    // Room for the shadow on every side (decision 018).
    const Tokens& t = tokens();
    margin_ = std::ceil(2 * t.shadowBlur + std::max(std::abs(t.shadowOffset.x), std::abs(t.shadowOffset.y)));

    // Like the island's render layer: DirectComposition owns every pixel,
    // and LAYERED | TRANSPARENT passes clicks through to other apps' windows.
    SetLastError(0); // so a failure's code is its own
    render_ = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                              kRenderClass, L"Cup Island Card", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    // An ordinary window, hit-tested by its region: draws nothing. Not
    // NOACTIVATE: a click gives the card the keys.
    input_ = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kInputClass, L"Cup Island Card", WS_POPUP, 0, 0, 1,
                             1, nullptr, nullptr, instance, this);
    if (!render_ || !input_) {
        logLine("failed to create a floating card's %s window (0x%08lx)", render_ ? "input" : "render", GetLastError());
        destroy();
        return false;
    }
    allowBackdrop(render_); // a card can be glass too (decision 089); where the island can't, it never is
    target_ = createWindowTarget(compositor, render_);
    root_ = compositor.CreateContainerVisual();
    target_.Root(root_);
    setCard(card, radius);
    return true;
}

void FloatWindow::destroy()
{
    callbacks_ = {}; // destroying sends focus messages: nothing reaches the app from a card that's going
    if (input_) {
        SetWindowLongPtrW(input_, GWLP_USERDATA, 0);
        DestroyWindow(std::exchange(input_, nullptr));
    }
    target_ = nullptr;
    root_ = nullptr;
    if (render_) DestroyWindow(std::exchange(render_, nullptr));
}

void FloatWindow::setCard(Rect card, float radius)
{
    bool reshaped = card.width != card_.width || card.height != card_.height || radius != radius_ || !shown_;
    card_ = card;
    radius_ = radius;
    // Both in one go, so the card and its clicks never part (decision 001).
    HDWP batch = BeginDeferWindowPos(2);
    if (batch)
        batch = DeferWindowPos(batch, render_, nullptr, px(card.x - margin_), px(card.y - margin_), px(card.width + 2 * margin_),
                               px(card.height + 2 * margin_), SWP_NOZORDER | SWP_NOACTIVATE);
    if (batch)
        batch = DeferWindowPos(batch, input_, nullptr, px(card.x), px(card.y), px(card.width), px(card.height), SWP_NOZORDER | SWP_NOACTIVATE);
    if (batch) EndDeferWindowPos(batch);
    if (root_) root_.Size({ card.width + 2 * margin_, card.height + 2 * margin_ });
    if (reshaped) applyRegion();
}

void FloatWindow::applyRegion()
{
    // The card's rounded shape: beside its corners, clicks go to whatever is
    // under (input client coordinates).
    int w = px(card_.width), h = px(card_.height), r = px(2 * radius_);
    SetWindowRgn(input_, CreateRoundRectRgn(0, 0, w + 1, h + 1, r, r), TRUE); // the window owns the region now
}

void FloatWindow::startMove()
{
    if (!pressed_) return;
    moving_ = true;
    GetCursorPos(&moveFrom_);
    moveCard_ = card_;
}

void FloatWindow::setResizable(bool resizable, Vec2 least, Vec2 most)
{
    resizable_ = resizable;
    least_ = least;
    most_ = most;
}

int FloatWindow::edgesAt(POINT client) const
{
    if (!resizable_) return 0;
    int band = px(tokens().floatResizeBorder), w = px(card_.width), h = px(card_.height);
    int edges = 0;
    if (client.x < band) edges |= kLeft;
    if (client.x >= w - band) edges |= kRight;
    if (client.y < band) edges |= kTop;
    if (client.y >= h - band) edges |= kBottom;
    return edges;
}

void FloatWindow::resizeTo(POINT cursor)
{
    // The edges held follow the pointer; the others stay put. Within its
    // limits: past them, the edge stops and the opposite one doesn't move.
    float dx = static_cast<float>(cursor.x - moveFrom_.x), dy = static_cast<float>(cursor.y - moveFrom_.y);
    Rect from = moveCard_, to = from;
    if (resizeEdges_ & kRight) to.width = std::clamp(from.width + dx, least_.x, most_.x);
    if (resizeEdges_ & kBottom) to.height = std::clamp(from.height + dy, least_.y, most_.y);
    if (resizeEdges_ & kLeft) {
        to.width = std::clamp(from.width - dx, least_.x, most_.x);
        to.x = from.x + from.width - to.width;
    }
    if (resizeEdges_ & kTop) {
        to.height = std::clamp(from.height - dy, least_.y, most_.y);
        to.y = from.y + from.height - to.height;
    }
    to = { std::round(to.x), std::round(to.y), std::round(to.width), std::round(to.height) };
    if (to == card_) return;
    setCard(to, radius_);
    call(callbacks_.onResized, Vec2{ to.width, to.height });
}

void FloatWindow::raise(bool focus)
{
    // The input window at the top of the topmost band, its drawing just under it.
    SetWindowPos(input_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(render_, input_, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (!focus) return;
    SetForegroundWindow(input_); // allowed: the user just clicked the island, or pressed its shortcut
    SetFocus(input_);
}

void FloatWindow::show(bool visible)
{
    if (visible == shown_) return;
    shown_ = visible;
    ShowWindow(render_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    ShowWindow(input_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void FloatWindow::hideInput()
{
    callbacks_ = {};
    if (input_) ShowWindow(input_, SW_HIDE);
}

void FloatWindow::setTextCursor(bool text)
{
    if (text == textCursor_) return;
    textCursor_ = text;
    POINT at{};
    if (GetCursorPos(&at) && WindowFromPoint(at) == input_) SetCursor(LoadCursorW(nullptr, text ? IDC_IBEAM : IDC_ARROW));
}

Vec2 FloatWindow::pointFrom(LPARAM lParam) const
{
    float shift = margin_; // the input window's top-left (the card's), in the render window
    return { static_cast<float>(GET_X_LPARAM(lParam)) + shift, static_cast<float>(GET_Y_LPARAM(lParam)) + shift };
}

LRESULT FloatWindow::handle(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto inside = [&] {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT client{};
        GetClientRect(hwnd, &client);
        return PtInRect(&client, pt) != FALSE;
    };
    switch (message) {
    case WM_LBUTTONDOWN:
        pressed_ = true;
        SetCapture(hwnd);
        if (int edges = edgesAt({ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) })) { // on an edge: resizing, not a click
            resizeEdges_ = edges;
            GetCursorPos(&moveFrom_);
            moveCard_ = card_;
            return 0;
        }
        call(callbacks_.onPressDown, pointFrom(lParam)); // may startMove()
        return 0;

    case WM_MOUSEMOVE:
        if (resizeEdges_) {
            POINT now{};
            GetCursorPos(&now);
            resizeTo(now);
            return 0;
        }
        if (moving_) {
            POINT now{};
            GetCursorPos(&now);
            setCard({ moveCard_.x + (now.x - moveFrom_.x), moveCard_.y + (now.y - moveFrom_.y), card_.width, card_.height }, radius_);
            return 0;
        }
        if (!trackingLeave_) {
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, hwnd, 0 };
            trackingLeave_ = TrackMouseEvent(&track) != FALSE;
        }
        call(callbacks_.onPointerMoved, pointFrom(lParam), inside());
        return 0;

    case WM_LBUTTONUP:
        if (!pressed_) return 0;
        pressed_ = false; // before ReleaseCapture, so its WM_CAPTURECHANGED isn't a cancel
        ReleaseCapture();
        if (std::exchange(resizeEdges_, 0) || std::exchange(moving_, false)) call(callbacks_.onMoved);
        else call(callbacks_.onPressRelease, pointFrom(lParam), inside() ? Release::Inside : Release::Outside);
        return 0;

    case WM_CAPTURECHANGED:
        if (!pressed_) return 0;
        pressed_ = false;
        if (std::exchange(resizeEdges_, 0) || std::exchange(moving_, false)) call(callbacks_.onMoved);
        else call(callbacks_.onPressRelease, Vec2{}, Release::Cancelled);
        return 0;

    case WM_SETCURSOR: {
        if (LOWORD(lParam) != HTCLIENT) break;
        // On a resizable card's edge: the resize arrows, as on any window's.
        POINT at{};
        GetCursorPos(&at);
        ScreenToClient(hwnd, &at);
        int edges = resizeEdges_ ? resizeEdges_ : edgesAt(at);
        const wchar_t* cursor = textCursor_ ? IDC_IBEAM : IDC_ARROW;
        if ((edges & (kLeft | kTop)) == (kLeft | kTop) || (edges & (kRight | kBottom)) == (kRight | kBottom)) cursor = IDC_SIZENWSE;
        else if ((edges & (kRight | kTop)) == (kRight | kTop) || (edges & (kLeft | kBottom)) == (kLeft | kBottom)) cursor = IDC_SIZENESW;
        else if (edges & (kLeft | kRight)) cursor = IDC_SIZEWE;
        else if (edges & (kTop | kBottom)) cursor = IDC_SIZENS;
        SetCursor(LoadCursorW(nullptr, cursor));
        return TRUE;
    }

    case WM_MOUSEWHEEL: {
        float notches = -static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; // screen coordinates, unlike the other mouse messages
        ScreenToClient(hwnd, &pt);
        if (notches != 0) call(callbacks_.onWheel, pointFrom(MAKELPARAM(pt.x, pt.y)), notches);
        return 0;
    }

    case WM_MOUSELEAVE:
        trackingLeave_ = false;
        call(callbacks_.onPointerLeft);
        return 0;

    case WM_SETFOCUS:
        call(callbacks_.onFocusChanged, true);
        return 0;
    case WM_KILLFOCUS:
        call(callbacks_.onFocusChanged, false);
        return 0;

    case WM_KEYDOWN:
        call(callbacks_.onKeyDown, wParam);
        return 0;
    case WM_SYSKEYDOWN:
        call(callbacks_.onKeyDown, wParam);
        break; // Windows' own handling still runs: Alt+F4 becomes SC_CLOSE below
    case WM_CHAR:
        call(callbacks_.onChar, static_cast<wchar_t>(wParam));
        return 0;

    case WM_SYSCOMMAND:
        // Alt+F4 sends the card back into the pill, unlike the island (decisions 049, 081).
        if ((wParam & 0xFFF0) == SC_CLOSE) {
            call(callbacks_.onCloseRequested);
            return 0;
        }
        break;
    case WM_CLOSE: // never destroyed behind the app's back
        call(callbacks_.onCloseRequested);
        return 0;

    case WM_DPICHANGED:
        // Ignored, not resized by Windows: the card is laid out at the
        // island's display scale for now (decision 081).
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace cup::platform
