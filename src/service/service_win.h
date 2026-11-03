#pragma once
// The index service's Windows headers (decision 075), lean: no WinRT, no
// Composition, unlike platform/win.h. Everything the service calls dates
// from Vista or earlier, so Windows 10 1809 is no concern (decision 015).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <winioctl.h>

#include <string>
#include <string_view>

namespace cup::service {

inline std::wstring widen(std::string_view utf8)
{
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

inline std::string narrow(std::wstring_view wide)
{
    if (wide.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), n, nullptr, nullptr);
    return out;
}

// Closes a handle when it goes out of scope; INVALID_HANDLE_VALUE and null
// both mean "none", since Win32 uses either depending on the call.
class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE h) : h_(h) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
    Handle& operator=(Handle&& o) noexcept
    {
        if (this != &o) {
            reset();
            h_ = o.h_;
            o.h_ = nullptr;
        }
        return *this;
    }
    HANDLE get() const { return h_; }
    bool valid() const { return h_ && h_ != INVALID_HANDLE_VALUE; }
    // Gives up ownership without closing: the caller closes it now.
    HANDLE release()
    {
        HANDLE h = h_;
        h_ = nullptr;
        return h;
    }
    void reset()
    {
        if (valid()) CloseHandle(h_);
        h_ = nullptr;
    }

private:
    HANDLE h_ = nullptr;
};

} // namespace cup::service
