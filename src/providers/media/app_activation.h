#pragma once
// Brings the app behind a media session to the front, and for a browser
// the tab that's playing: what clicking the expanded art does (decision
// 005). Windows only. Call right after the click, while we may still hand
// over the foreground; the work runs on a background thread and logs its result.

#include <string>

namespace cup {

// appId: the session's SourceAppUserModelId ("Spotify.exe", "Chrome",
// "Microsoft.ZuneMusic_8wekyb3d8bbwe!Microsoft.ZuneMusic"…). title: what's
// playing. A window already showing it (its active tab is the media) is
// raised as is; otherwise the app's tabs are searched for it (browser_tabs).
void openSourceApp(const std::wstring& appId, const std::wstring& title);

} // namespace cup
