#include "providers/demo/demo_media_provider.h"

#include <cstdint>

namespace cup {
namespace {

// A diagonal two-colour gradient, opaque BGRA.
std::shared_ptr<const ImageData> gradient(std::uint32_t from, std::uint32_t to)
{
    constexpr int kSize = 64;
    auto image = std::make_shared<ImageData>();
    image->width = image->height = kSize;
    image->bgra.resize(kSize * kSize * 4);
    auto channel = [](std::uint32_t rgb, int shift) { return static_cast<float>((rgb >> shift) & 0xFF); };
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            float t = static_cast<float>(x + y) / (2 * (kSize - 1));
            std::uint8_t* px = &image->bgra[static_cast<size_t>((y * kSize + x) * 4)];
            for (int c = 0; c < 3; ++c) { // B, G, R
                int shift = c * 8;
                px[c] = static_cast<std::uint8_t>(channel(from, shift) + (channel(to, shift) - channel(from, shift)) * t);
            }
            px[3] = 255;
        }
    }
    return image;
}

} // namespace

DemoMediaProvider::DemoMediaProvider()
    : tracks_{ { "Ocean Drive", "Cup Island Demo", Ms{ 200000 }, gradient(0x2E86DE, 0x10AC84) },
               { "Midnight Tea", "The Placeholders", Ms{ 165000 }, gradient(0xEE5253, 0x5F27CD) },
               { "A Very Long Song Title That Will Not Fit", "Someone With A Long Name", Ms{ 3725000 }, gradient(0xFF9F43, 0xEE5253) } }
{
}

void DemoMediaProvider::start(Host& host)
{
    host_ = &host;
    timeline_ = { .position = Ms{ 0 }, .duration = tracks_[track_].duration, .anchoredAt = host.now(), .playing = true };
    publish();
}

void DemoMediaProvider::stop()
{
    if (host_) host_->endActivity(kActivityId);
    host_ = nullptr;
}

void DemoMediaProvider::onAction(const Action& action)
{
    if (!host_) return;
    namespace act = media_action;
    Ms now = host_->now();
    if (action.actionId == act::kPlayPause) {
        timeline_.position = timeline_.positionAt(now);
        timeline_.anchoredAt = now;
        timeline_.playing = !timeline_.playing;
    } else if (action.actionId == act::kNext) {
        changeTrack(+1);
    } else if (action.actionId == act::kPrevious) {
        changeTrack(-1);
    } else if (action.actionId == act::kSeek) {
        timeline_.position = Ms{ static_cast<long long>(static_cast<double>(timeline_.duration.count()) * action.value) };
        timeline_.anchoredAt = now;
    }
    publish();
}

void DemoMediaProvider::changeTrack(int delta)
{
    auto count = static_cast<int>(tracks_.size());
    track_ = static_cast<size_t>((static_cast<int>(track_) + delta + count) % count);
    timeline_ = { .position = Ms{ 0 }, .duration = tracks_[track_].duration, .anchoredAt = host_->now(), .playing = true };

    // Same coalesce key as the real media provider: skipping fast shows one alert.
    NowPlaying m{ .title = tracks_[track_].title, .artist = tracks_[track_].artist, .art = tracks_[track_].art, .timeline = timeline_ };
    host_->postAlert({ .activityId = kActivityId, .content = nowPlayingContent(m), .coalesceKey = "track" });
}

void DemoMediaProvider::publish()
{
    const Track& track = tracks_[track_];
    NowPlaying m{ .title = track.title, .artist = track.artist, .source = "Demo player", .art = track.art,
                  .timeline = timeline_, .canSeek = true };
    host_->startActivity({ .id = kActivityId, .content = nowPlayingContent(m) });
}

} // namespace cup
