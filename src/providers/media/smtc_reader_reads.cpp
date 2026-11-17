// SmtcReader's full read of the shown session, and the media buttons: the
// rest of smtc_reader.cpp, on the reader's thread too (decision 144).
#include "providers/media/smtc_reader.h"
#include "platform/log.h"

#include <utility>

namespace cup {

winrt::fire_and_forget SmtcReader::load()
{
    auto self = shared_from_this(); // held across the awaits
    Session session = session_;
    if (!session) {
        out_.show(std::nullopt);
        co_return;
    }
    loading_ = true;
    loadStarted_ = out_.now();
    // However the read ends, the next one asked for meanwhile starts (on the reader's thread, where this resumes).
    struct Done {
        SmtcReader* reader;
        ~Done()
        {
            if (!reader->stopped_) reader->guarded([](SmtcReader& r) { r.loaded(); });
        }
    } done{ this };

    Properties properties{ nullptr };
    Ms began = loadStarted_;
    // Taken before the read: a properties event arriving during it (Spotify's
    // thumbnail, just after its title) stays set for the read that follows.
    bool propertiesChanged = std::exchange(propertiesDirty_, false);
    std::optional<winrt::hresult> failed;
    try {
        properties = co_await session.TryGetMediaPropertiesAsync();
    } catch (winrt::hresult_error const& e) {
        failed = e.code(); // after sleep, thrown on a pool thread (0x80010100): handled below, on the reader's thread
    }
    co_await onReader(); // decision 060
    if (stopped_) co_return;
    // Everything from here calls into the session, and its app can close it at
    // any moment (Spotify quitting): any call can throw. Nothing may escape:
    // out of a fire_and_forget it is a fail-fast, and the island vanished with
    // nothing in the log (decision 158).
    try {
        if (failed) winrt::throw_hresult(*failed);
        if (session != session_) co_return; // another session is shown now: its read follows
        showRead(session, properties, propertiesChanged, began);
    } catch (winrt::hresult_error const& e) {
        propertiesDirty_ = propertiesDirty_ || propertiesChanged; // not read: the next read still owes it
        // The app closed its session, or is mid-switch (Chrome changing tabs)
        // and may send nothing more: try again shortly, a few times.
        if (session == session_ && readFailures_++ < tokens_.mediaReadRetries) {
            platform::logLine("media: read failed (0x%08x), retrying", static_cast<unsigned>(e.code()));
            refreshLater(tokens_.mediaSessionRecheck, false);
        }
    } catch (std::exception const& e) {
        platform::logLine("media: error while reading (%s)", e.what());
    }
}

void SmtcReader::showRead(Session const& session, Properties const& properties, bool propertiesChanged, Ms began)
{
    NowPlaying media = readNowPlaying(properties, session.GetPlaybackInfo(), session.GetTimelineProperties(), out_.now());
    readFailures_ = 0;
    if (Ms took = out_.now() - began; took > Ms{ 250 })
        platform::logLine("media: read took %lld ms", static_cast<long long>(took.count()));
    if (shownPlaying_ != media.timeline.playing) {
        if (shownPlaying_) platform::logLine("media: %s", media.timeline.playing ? "playing" : "paused");
        shownPlaying_ = media.timeline.playing;
    }

    winrt::hstring app = session.SourceAppUserModelId();
    media.source = appName(app);
    media.showSourceSwitch = manager_.GetSessions().Size() > 1;
    media.canOpenSource = true;

    // The cover (ArtState): decoded on a new track and on every properties
    // update (Chrome sends a tab's picture after its title). A new track
    // shows at once, without one; the cover fades in when it's read (056).
    auto thumbnail = properties.Thumbnail();
    std::string track = winrt::to_string(app) + "|" + media.title + "|" + media.artist + "|" + winrt::to_string(properties.AlbumTitle());
    bool newTrack = track != loggedTrack_;
    if (newTrack) {
        loggedTrack_ = track;
        platform::logLine("media: new track%s", thumbnail ? "" : " (no cover yet)");
    }
    if (auto id = art_.onRead(track, thumbnail != nullptr, propertiesChanged)) {
        decodeArt(thumbnail, [weak = weak_from_this(), id = *id](std::shared_ptr<const ImageData> image) {
            // Decoding ends on whatever thread Windows chose (decision 060): back to the reader's.
            if (auto reader = weak.lock()) reader->post([id, image](SmtcReader& r) { r.onArtDecoded(id, image); });
        });
        if (newTrack) refreshLater(tokens_.mediaArtRecheck, true); // the right picture may come in a later update
    }

    recheckArtIfMissing(); // no picture yet: look again, in case no event says it came
    media.art = art_.art();
    out_.show(MediaSnapshot{ winrt::to_string(app), media });
}

std::vector<SessionView> SmtcReader::describe(Session const& remembered) const
{
    std::vector<SessionView> views;
    for (auto const& s : manager_.GetSessions())
        views.push_back({ winrt::to_string(s.SourceAppUserModelId()), remembered && s == remembered });
    return views;
}

void SmtcReader::cycleSource()
{
    auto sessions = manager_.GetSessions();
    std::uint32_t count = sessions.Size();
    if (count < 2) return;
    // Found by object, else by app id: an identity-only search could miss the
    // shown session and "switch" to it again, so the first click did nothing.
    std::string shownApp = session_ ? winrt::to_string(session_.SourceAppUserModelId()) : "";
    auto next = nextSession(count, findSession(describe(session_), shownApp));
    chosen_ = sessions.GetAt(static_cast<std::uint32_t>(next));
    chosenApp_ = winrt::to_string(chosen_.SourceAppUserModelId());
    platform::logLine("media: source button -> %s", appName(chosen_.SourceAppUserModelId()).c_str());
    pickSession();
}

void SmtcReader::onAction(const Action& action)
{
    if (!session_) return;
    namespace act = media_action;
    try {
        // Fire and forget: the real state comes back through the change events.
        if (action.actionId == act::kPlayPause) session_.TryTogglePlayPauseAsync();
        else if (action.actionId == act::kNext) session_.TrySkipNextAsync();
        else if (action.actionId == act::kPrevious) session_.TrySkipPreviousAsync();
        else if (action.actionId == act::kSource) cycleSource();
        else if (action.actionId == act::kShuffle) {
            auto shuffle = session_.GetPlaybackInfo().IsShuffleActive();
            session_.TryChangeShuffleActiveAsync(!(shuffle && shuffle.Value()));
        } else if (action.actionId == act::kRepeat) {
            auto repeat = session_.GetPlaybackInfo().AutoRepeatMode();
            session_.TryChangeAutoRepeatModeAsync(toWindows(nextRepeat(repeat ? fromWindows(repeat.Value()) : Repeat::Off)));
        }
        else if (action.actionId == act::kSeek) {
            auto timeline = session_.GetTimelineProperties();
            auto start = timeline.StartTime(), end = timeline.EndTime();
            auto offset = static_cast<std::int64_t>(static_cast<double>((end - start).count()) * action.value);
            session_.TryChangePlaybackPositionAsync(start.count() + offset); // 100 ns ticks
        }
    } catch (winrt::hresult_error const&) {
        // Session vanished between the click and the call.
    }
}

} // namespace cup
