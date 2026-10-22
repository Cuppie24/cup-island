#include "platform/wall_clock.h"

#include "platform/win.h"

#include <cwchar>

namespace cup::platform {

ClockSynced readWallClock()
{
    SYSTEMTIME t{};
    GetLocalTime(&t);
    Ms timeOfDay{ ((t.wHour * 60LL + t.wMinute) * 60 + t.wSecond) * 1000 + t.wMilliseconds };
    wchar_t format[80]{};
    bool hour24 = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, format, 80) == 0 || wcschr(format, L'H') != nullptr;
    return { timeOfDay, hour24 };
}

Ms utcOffset()
{
    // Bias is UTC minus local, in minutes; the season's bias goes on top. Since Windows 2000.
    TIME_ZONE_INFORMATION zone{};
    DWORD which = GetTimeZoneInformation(&zone);
    long bias = zone.Bias;
    if (which == TIME_ZONE_ID_DAYLIGHT) bias += zone.DaylightBias;
    else if (which == TIME_ZONE_ID_STANDARD) bias += zone.StandardBias;
    return Ms{ -bias * 60000LL };
}

} // namespace cup::platform
