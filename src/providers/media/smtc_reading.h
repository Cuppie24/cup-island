#pragma once
// Reading Windows' media sessions (SMTC, TR §6) into plain NowPlaying data.
// Stateless helpers; the provider owns sessions and threading.

#include "providers/media/now_playing.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <functional>
#include <memory>
#include <string>

namespace cup {

namespace wmc = winrt::Windows::Media::Control;

// Title, artist, controls and timeline. The position is anchored on `appNow`
// using the session's LastUpdatedTime, so the island extrapolates it exactly.
NowPlaying readNowPlaying(wmc::GlobalSystemMediaTransportControlsSessionMediaProperties const& properties,
                          wmc::GlobalSystemMediaTransportControlsSessionPlaybackInfo const& playback,
                          wmc::GlobalSystemMediaTransportControlsSessionTimelineProperties const& timeline, Ms appNow);

// Display name of the app that owns a session ("Spotify", "Google Chrome"…). Cached.
std::string appName(winrt::hstring const& appUserModelId);

// Decodes album art to premultiplied BGRA, at most 128 px on the long side.
// Start it on the UI thread: `done` then runs there too, with nullptr on failure.
winrt::fire_and_forget decodeArt(winrt::Windows::Storage::Streams::IRandomAccessStreamReference thumbnail,
                                 std::function<void(std::shared_ptr<const ImageData>)> done);

} // namespace cup
