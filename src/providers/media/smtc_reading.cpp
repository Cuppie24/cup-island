#include "providers/media/smtc_reading.h"

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Graphics.Imaging.h>

#include <algorithm>
#include <cctype>
#include <map>

namespace cup {
namespace {

using namespace winrt::Windows::Graphics::Imaging;
using Status = wmc::GlobalSystemMediaTransportControlsSessionPlaybackStatus;

constexpr std::uint32_t kMaxArtSize = 128;

} // namespace

NowPlaying readNowPlaying(wmc::GlobalSystemMediaTransportControlsSessionMediaProperties const& properties,
                          wmc::GlobalSystemMediaTransportControlsSessionPlaybackInfo const& playback,
                          wmc::GlobalSystemMediaTransportControlsSessionTimelineProperties const& timeline, Ms appNow)
{
    NowPlaying m;
    m.title = winrt::to_string(properties.Title());
    m.artist = winrt::to_string(properties.Artist());

    auto controls = playback.Controls();
    m.canPrevious = controls.IsPreviousEnabled();
    m.canNext = controls.IsNextEnabled();
    m.canPlayPause = controls.IsPlayPauseToggleEnabled() || controls.IsPlayEnabled() || controls.IsPauseEnabled();

    Timeline& t = m.timeline;
    t.playing = playback.PlaybackStatus() == Status::Playing;
    auto rate = playback.PlaybackRate();
    t.rate = rate && rate.Value() > 0 ? static_cast<float>(rate.Value()) : 1.0f;

    auto start = timeline.StartTime(), end = timeline.EndTime();
    if (end > start) { // apps without a timeline (some browsers, radio) report 0
        t.duration = std::chrono::duration_cast<Ms>(end - start);
        t.position = std::chrono::duration_cast<Ms>(timeline.Position() - start);
    }
    // Position was true at LastUpdatedTime (wall clock); translate that to app time.
    auto updated = timeline.LastUpdatedTime();
    auto age = winrt::clock::now() - updated;
    if (updated.time_since_epoch().count() == 0 || age.count() < 0) age = {};
    t.anchoredAt = appNow - std::chrono::duration_cast<Ms>(age);

    m.canSeek = controls.IsPlaybackPositionEnabled() && t.duration > Ms{ 0 };
    return m;
}

std::string appName(winrt::hstring const& appUserModelId)
{
    static std::map<winrt::hstring, std::string> cache;
    if (auto it = cache.find(appUserModelId); it != cache.end()) return it->second;

    std::string name;
    try {
        // Packaged apps (Media Player, browsers from the Store). Win32 apps throw here.
        name = winrt::to_string(winrt::Windows::ApplicationModel::AppInfo::GetFromAppUserModelId(appUserModelId)
                                    .DisplayInfo().DisplayName());
    } catch (winrt::hresult_error const&) {
    }
    if (name.empty()) {
        // "Spotify.exe" -> "Spotify"; "Publisher.App_hash!App" -> "App".
        name = winrt::to_string(appUserModelId);
        if (auto bang = name.rfind('!'); bang != std::string::npos) name = name.substr(bang + 1);
        if (name.size() > 4) {
            std::string tail = name.substr(name.size() - 4);
            std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (tail == ".exe") name.resize(name.size() - 4);
        }
    }
    cache.emplace(appUserModelId, name);
    return name;
}

winrt::fire_and_forget decodeArt(winrt::Windows::Storage::Streams::IRandomAccessStreamReference thumbnail,
                                 std::function<void(std::shared_ptr<const ImageData>)> done)
{
    std::shared_ptr<const ImageData> result;
    try {
        auto stream = co_await thumbnail.OpenReadAsync();
        auto decoder = co_await BitmapDecoder::CreateAsync(stream);
        std::uint32_t width = decoder.PixelWidth(), height = decoder.PixelHeight();
        double scale = std::min(1.0, static_cast<double>(kMaxArtSize) / std::max<std::uint32_t>({ width, height, 1 }));
        BitmapTransform transform;
        transform.ScaledWidth(std::max<std::uint32_t>(1, static_cast<std::uint32_t>(width * scale)));
        transform.ScaledHeight(std::max<std::uint32_t>(1, static_cast<std::uint32_t>(height * scale)));
        transform.InterpolationMode(BitmapInterpolationMode::Fant);

        // Album art practically never carries EXIF rotation; ignoring it keeps
        // the output size equal to the scaled size.
        auto pixels = co_await decoder.GetPixelDataAsync(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied, transform,
                                                         ExifOrientationMode::IgnoreExifOrientation,
                                                         ColorManagementMode::DoNotColorManage);
        winrt::com_array<std::uint8_t> bytes = pixels.DetachPixelData();

        auto image = std::make_shared<ImageData>();
        image->width = static_cast<int>(transform.ScaledWidth());
        image->height = static_cast<int>(transform.ScaledHeight());
        image->bgra.assign(bytes.begin(), bytes.end());
        if (image->bgra.size() == static_cast<size_t>(image->width) * static_cast<size_t>(image->height) * 4) result = image;
    } catch (winrt::hresult_error const&) {
        // Unreadable or unsupported art: show the track without it.
    }
    done(result);
}

} // namespace cup
