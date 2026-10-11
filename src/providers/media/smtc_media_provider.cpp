#include "providers/media/smtc_media_provider.h"
#include "platform/log.h"

#include "core/tokens.h"

#include <cstdio>

namespace cup {

auto SmtcMediaProvider::onUiThread(void (SmtcMediaProvider::*method)())
{
    return [this, alive = alive_, method](auto&&...) {
        post_([this, alive, method] {
            if (*alive) guarded(method);
        });
    };
}

void SmtcMediaProvider::guarded(void (SmtcMediaProvider::*method)())
{
    // A session can vanish while we read it (the app closed): its calls then
    // throw. Uncaught in a posted callback, that would end the app; the next
    // SessionsChanged brings us back in sync.
    try {
        (this->*method)();
    } catch (winrt::hresult_error const& e) {
        platform::logLine("media: session went away mid-update (0x%08x)", static_cast<unsigned>(e.code()));
    }
}

void SmtcMediaProvider::start(Host& host)
{
    host_ = &host;
    presenter_ = std::make_unique<MediaPresenter>(host);
    presenter_->start();
    alive_ = std::make_shared<bool>(true);
    connect(alive_);
}

void SmtcMediaProvider::stop()
{
    *alive_ = false;
    currentChanged_.revoke();
    sessionsChanged_.revoke();
    propertiesChanged_.revoke();
    timelineChanged_.revoke();
    playbackWatchers_.clear();
    manager_ = nullptr;
    session_ = nullptr;
    chosen_ = nullptr;
    chosenApp_.clear();
    mainApp_.clear();
    artKey_.clear();
    artPending_ = false;
    art_.reset();
    presenter_->stop();
    presenter_.reset();
    host_ = nullptr;
}

winrt::fire_and_forget SmtcMediaProvider::connect(std::shared_ptr<bool> alive)
{
    Manager manager{ nullptr };
    try {
        manager = co_await Manager::RequestAsync();
    } catch (winrt::hresult_error const& e) {
        platform::logLine("media: session manager unavailable (0x%08x)", static_cast<unsigned>(e.code()));
        co_return;
    }
    if (!*alive) co_return;

    manager_ = manager;
    currentChanged_ = manager_.CurrentSessionChanged(winrt::auto_revoke, onUiThread(&SmtcMediaProvider::pickSession));
    sessionsChanged_ = manager_.SessionsChanged(winrt::auto_revoke, onUiThread(&SmtcMediaProvider::onSessionsChanged));
    platform::logLine("media: %u session(s)", manager_.GetSessions().Size());
    onSessionsChanged();
}

void SmtcMediaProvider::onSessionsChanged()
{
    watchAllSessions();
    pickSession();
}

void SmtcMediaProvider::watchAllSessions()
{
    playbackWatchers_.clear(); // revokes the old subscriptions
    for (auto const& s : manager_.GetSessions())
        playbackWatchers_.push_back(s.PlaybackInfoChanged(winrt::auto_revoke, onUiThread(&SmtcMediaProvider::onPlaybackChanged)));
}

void SmtcMediaProvider::onPlaybackChanged()
{
    pickSession();
    recheckSoon(alive_);
}

winrt::fire_and_forget SmtcMediaProvider::recheckSoon(std::shared_ptr<bool> alive)
{
    auto post = post_; // `this` is only touched back on the UI thread, after the alive check
    co_await winrt::resume_after(tokens().mediaSessionRecheck);
    post([this, alive] {
        if (*alive) guarded(&SmtcMediaProvider::pickSession);
    });
}

void SmtcMediaProvider::pickSession()
{
    if (!manager_) return;
    auto sessions = manager_.GetSessions();

    // When Windows moves its main media to another app (the one the media keys
    // now control), follow it, dropping any manual pick. Compared by app id:
    // a new object for the same session, or a moment with no main media, isn't a move.
    Session current = manager_.GetCurrentSession();
    std::string currentApp = current ? winrt::to_string(current.SourceAppUserModelId()) : "";
    if (mainMediaMoved(mainApp_, currentApp)) {
        mainApp_ = currentApp;
        if (chosen_) platform::logLine("media: manual pick dropped, Windows moved its main media");
        chosen_ = nullptr;
        chosenApp_.clear();
        platform::logLine("media: Windows' main media is %s", appName(current.SourceAppUserModelId()).c_str());
    }

    // By session first (every Chrome tab shares one app id), app id as fallback.
    Session chosen{ nullptr };
    if (chosen_) {
        if (auto i = findSession(describe(chosen_), chosenApp_)) {
            chosen = sessions.GetAt(static_cast<std::uint32_t>(*i));
            chosen_ = chosen; // keep the object Windows hands out now
        } else {
            platform::logLine("media: picked session went away, following Windows again");
            chosen_ = nullptr;
            chosenApp_.clear();
        }
    }
    if (!chosen) chosen = current;
    if (!chosen && sessions.Size() > 0) chosen = sessions.GetAt(0);

    if (chosen != session_) {
        session_ = chosen;
        propertiesChanged_.revoke();
        timelineChanged_.revoke();
        if (session_) {
            // (Playback changes are watched for every session in watchAllSessions.)
            propertiesChanged_ = session_.MediaPropertiesChanged(winrt::auto_revoke, onUiThread(&SmtcMediaProvider::refresh));
            timelineChanged_ = session_.TimelinePropertiesChanged(winrt::auto_revoke, onUiThread(&SmtcMediaProvider::refresh));
            platform::logLine("media: showing %s", appName(session_.SourceAppUserModelId()).c_str());
        }
    }
    refresh(); // also when only the session count changed (source button)
}

void SmtcMediaProvider::refresh()
{
    load(++generation_, alive_);
}

winrt::fire_and_forget SmtcMediaProvider::load(std::uint64_t generation, std::shared_ptr<bool> alive)
{
    Session session = session_;
    if (!session) {
        presenter_->show(std::nullopt);
        co_return;
    }

    wmc::GlobalSystemMediaTransportControlsSessionMediaProperties properties{ nullptr };
    NowPlaying media;
    try {
        properties = co_await session.TryGetMediaPropertiesAsync();
        if (!*alive || generation != generation_) co_return; // stopped, or a newer refresh started
        media = readNowPlaying(properties, session.GetPlaybackInfo(), session.GetTimelineProperties(), host_->now());
    } catch (winrt::hresult_error const&) {
        co_return; // the app closed its session mid-read; SessionsChanged follows
    }

    winrt::hstring app = session.SourceAppUserModelId();
    media.source = appName(app);
    media.showSourceSwitch = manager_.GetSessions().Size() > 1;

    // New track (or its art just appeared): decode the art before showing, so
    // the track alert carries it. Other refreshes wait for the decode too.
    auto thumbnail = properties.Thumbnail();
    std::string key = winrt::to_string(app) + "|" + media.title + "|" + media.artist + "|" +
                      winrt::to_string(properties.AlbumTitle()) + "|" + (thumbnail ? "art" : "");
    if (key != artKey_) {
        artKey_ = key;
        art_.reset();
        artPending_ = thumbnail != nullptr;
        if (artPending_) {
            decodeArt(thumbnail, [this, alive, key](std::shared_ptr<const ImageData> image) {
                if (!*alive || key != artKey_) return; // a newer track took over
                art_ = std::move(image);
                artPending_ = false;
                refresh();
            });
        }
    }
    if (artPending_) co_return;

    media.art = art_;
    presenter_->show(MediaSnapshot{ winrt::to_string(app), media });
}

std::vector<SessionView> SmtcMediaProvider::describe(Session const& remembered) const
{
    std::vector<SessionView> views;
    for (auto const& s : manager_.GetSessions())
        views.push_back({ winrt::to_string(s.SourceAppUserModelId()), remembered && s == remembered });
    return views;
}

void SmtcMediaProvider::cycleSource()
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

void SmtcMediaProvider::onAction(const Action& action)
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
