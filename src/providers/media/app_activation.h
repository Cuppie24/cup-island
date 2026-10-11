#pragma once
// Brings the app behind a media session to the front: what clicking the
// expanded art does (decision 005). Windows only; call on the UI thread
// right after the click, while we're allowed to hand over the foreground.

#include <string>

namespace cup {

// appId: the session's SourceAppUserModelId ("Spotify.exe", "Chrome",
// "Microsoft.ZuneMusic_8wekyb3d8bbwe!Microsoft.ZuneMusic"…). title: what's
// playing; among several windows of the app, one whose title shows it (a
// browser window whose active tab is the media) is preferred. False if
// nothing could be brought forward or started.
bool openSourceApp(const std::wstring& appId, const std::wstring& title);

} // namespace cup
