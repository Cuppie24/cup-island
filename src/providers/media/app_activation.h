#pragma once
// Brings the app behind a media session to the front, and for a browser
// the tab that's playing: what clicking the expanded art does (decision
// 005). Windows only. Call right after the click, while we may still hand
// over the foreground; the work runs on a background thread and logs its result.

#include <string>
#include <vector>

namespace cup {

// The exe names (lower case, no ".exe") the app behind a session id may run
// as: the id's own, when it names one ("Spotify.exe", "Chrome"), and its
// Start menu entry's (apps that pick their own id). None for a Store app
// ("Package_hash!App"): its processes carry the id itself.
std::vector<std::wstring> appExeStems(const std::wstring& appId);
// A running process's exe name, the same way; "" if it can't be asked.
std::wstring exeStemOfProcess(unsigned long pid);

// appId: the session's SourceAppUserModelId ("Spotify.exe", "Chrome",
// "Microsoft.ZuneMusic_8wekyb3d8bbwe!Microsoft.ZuneMusic"…). title: what's
// playing. A window already showing it (its active tab is the media) is
// raised as is; otherwise the app's tabs are searched for it (browser_tabs).
void openSourceApp(const std::wstring& appId, const std::wstring& title);

} // namespace cup
