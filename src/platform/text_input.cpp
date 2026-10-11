#include "platform/text_input.h"

#include "platform/win.h"

#include <algorithm>

namespace cup::platform {

std::string toUtf8(std::wstring_view text)
{
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(std::max(size, 0)), '\0');
    if (size > 0) WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::string readClipboardText(void* ownerHwnd)
{
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(static_cast<HWND>(ownerHwnd))) return {};
    std::string text;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        if (auto* chars = static_cast<const wchar_t*>(GlobalLock(data))) {
            text = toUtf8(chars);
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return text;
}

} // namespace cup::platform
