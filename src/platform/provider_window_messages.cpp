// A provider's window's messages (decision 104): its frame (ours, all of
// it), the pointer, keys and closing, each reported to the app. Split out of
// provider_window.cpp, which draws it.

#include "platform/provider_window.h"
#include "platform/log.h"

#include <cmath>
#include <exception>
#include <utility>

namespace cup::platform {
namespace {

Vec2 pointOf(LPARAM lParam) { return { static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)) }; }

} // namespace

LRESULT CALLBACK ProviderWindow::proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        static_cast<ProviderWindow*>(create->lpCreateParams)->hwnd_ = hwnd; // messages arrive before CreateWindowEx returns
    }
    auto* self = reinterpret_cast<ProviderWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, message, wParam, lParam);
    try {
        return self->handle(message, wParam, lParam);
    } catch (winrt::hresult_error const& e) {
        logLine("WinRT error in a provider's window, message 0x%04x: 0x%08x %ls", message, static_cast<unsigned int>(e.code()), e.message().c_str());
    } catch (std::exception const& e) {
        logLine("Exception in a provider's window, message 0x%04x: %s", message, e.what());
    } catch (...) {
        logLine("Unknown exception in a provider's window, message 0x%04x", message);
    }
    return 0;
}

LRESULT ProviderWindow::handle(UINT message, WPARAM wParam, LPARAM lParam)
{
    auto call = [](const auto& f, auto&&... args) {
        if (f) f(std::forward<decltype(args)>(args)...);
    };
    switch (message) {
    case WM_NCCALCSIZE:
        if (wParam) {
            // The whole window is client area: our title bar is the top of it.
            // Maximized, Windows puts the frame off screen: keep the content on it.
            if (IsZoomed(hwnd_)) {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                int frame = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                InflateRect(&params->rgrc[0], -frame, -frame);
            }
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd_, &p);
        RECT r{};
        GetClientRect(hwnd_, &r);
        if (resizable_ && !IsZoomed(hwnd_)) {
            int band = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            bool left = p.x < band, right = p.x >= r.right - band, top = p.y < band, bottom = p.y >= r.bottom - band;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
        }
        if (callbacks_.isCaption && callbacks_.isCaption({ static_cast<float>(p.x), static_cast<float>(p.y) })) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = { static_cast<LONG>(std::lround(minSize_.x)), static_cast<LONG>(std::lround(minSize_.y)) };
        return 0;
    }
    case WM_SIZE:
        rememberNormal();
        if (root_ && wParam != SIZE_MINIMIZED) // not while CreateWindowEx is still building it
            call(callbacks_.onResized, Vec2{ static_cast<float>(LOWORD(lParam)), static_cast<float>(HIWORD(lParam)) });
        return 0;
    case WM_MOVE:
        rememberNormal();
        return 0;
    case WM_EXITSIZEMOVE:
        call(callbacks_.onMoved);
        return 0;
    case WM_ERASEBKGND:
        return 1; // Composition draws all of it
    case WM_MOUSEMOVE:
        if (!trackingLeave_) {
            TRACKMOUSEEVENT track{ sizeof track, TME_LEAVE, hwnd_, 0 };
            trackingLeave_ = TrackMouseEvent(&track) != FALSE;
        }
        call(callbacks_.onPointerMoved, pointOf(lParam));
        return 0;
    case WM_MOUSELEAVE:
        trackingLeave_ = false;
        call(callbacks_.onPointerLeft);
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd_);
        pressed_ = true;
        call(callbacks_.onPressDown, pointOf(lParam));
        return 0;
    case WM_LBUTTONUP: {
        if (!pressed_) return 0;
        pressed_ = false;
        Vec2 p = pointOf(lParam);
        Vec2 size = clientSize();
        ReleaseCapture();
        call(callbacks_.onPressRelease, p, p.x >= 0 && p.y >= 0 && p.x < size.x && p.y < size.y);
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (pressed_) { // taken mid-press by someone else: nothing is clicked
            pressed_ = false;
            call(callbacks_.onPressRelease, Vec2{ -1, -1 }, false);
        }
        return 0;
    case WM_MOUSEWHEEL: {
        POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd_, &p);
        call(callbacks_.onWheel, Vec2{ static_cast<float>(p.x), static_cast<float>(p.y) },
             static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
        return 0;
    }
    case WM_KEYDOWN:
        call(callbacks_.onKeyDown, wParam);
        return 0;
    case WM_SYSKEYDOWN:
        if (wParam == VK_F4) { // Alt+F4: closed by the app, as glass leaves no system menu to send it
            call(callbacks_.onCloseRequested);
            return 0;
        }
        call(callbacks_.onKeyDown, wParam);
        break;
    case WM_NCLBUTTONDBLCLK:
        // Not maximizable (WindowSpec): a double click on the title bar doesn't
        // maximize it, nor one on its top or bottom edge stretch it to the screen's height.
        if (!maximizable_ && (wParam == HTCAPTION || wParam == HTTOP || wParam == HTBOTTOM)) return 0;
        break;
    case WM_SYSCOMMAND:
        if (!maximizable_ && ((wParam & 0xFFF0) == SC_MAXIMIZE)) return 0; // Win+Up, the system menu
        break;
    case kRevealMessage:
        reveal();
        return 0;
    case WM_TIMER:
        if (wParam == kRevealTimer) reveal();
        return 0;
    case WM_CLOSE:
        call(callbacks_.onCloseRequested); // the app closes it, through its provider's registry
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace cup::platform
