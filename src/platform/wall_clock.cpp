#include "platform/wall_clock.h"

#include "platform/win.h"

#include <algorithm>
#include <cwchar>
#include <string>

namespace cup::platform {

ClockSynced readWallClock()
{
    SYSTEMTIME t{};
    GetLocalTime(&t);
    Ms timeOfDay{ ((t.wHour * 60LL + t.wMinute) * 60 + t.wSecond) * 1000 + t.wMilliseconds };
    wchar_t format[80]{};
    bool hour24 = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, format, 80) == 0 || wcschr(format, L'H') != nullptr;
    wchar_t date[80]{};
    std::string utf8;
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &t, L"ddd d MMM", date, 80, nullptr) > 0) {
        int size = WideCharToMultiByte(CP_UTF8, 0, date, -1, nullptr, 0, nullptr, nullptr); // with the terminator
        utf8.resize(static_cast<std::size_t>(std::max(size, 1)));
        WideCharToMultiByte(CP_UTF8, 0, date, -1, utf8.data(), size, nullptr, nullptr);
        utf8.pop_back();
    }
    return { timeOfDay, hour24, utf8 };
}

} // namespace cup::platform
