#include "providers/media/smtc_reader.h"
#include "platform/log.h"

#include <algorithm>
#include <utility>

namespace cup {

std::shared_ptr<SmtcReader> SmtcReader::start(Out out, const Tokens& settings)
{
    std::shared_ptr<SmtcReader> reader(new SmtcReader(std::move(out), settings));
    // Windows 10 1709+ (decision 015: 1809 is the floor).
    reader->controller_ = winrt::Windows::System::DispatcherQueueController::CreateOnDedicatedThread();
    reader->queue_ = reader->controller_.DispatcherQueue();
    reader->post([](SmtcReader& r) {
        r.thread_ = std::this_thread::get_id();
        // The queue's thread normally has COM already; if so this is a no-op,
        // and if it has another apartment that one is fine too.
        try {
            winrt::init_apartment(winrt::apartment_type::single_threaded);
        } catch (winrt::hresult_error const&) {
        }
        r.connect();
    });
    return reader;
}

void SmtcReader::stop()
{
    stopped_ = true;
    // Straight onto the queue: post() would skip it, now that it's stopped.
    queue_.TryEnqueue([self = shared_from_this()] { self->disconnect(); });
    // The thread ends once the queue has drained. If a call into Windows
    // never returns, it never does: a parked thread, and the island goes on.
    controller_.ShutdownQueueAsync();
}

void SmtcReader::act(const Action& action)
{
    post([action](SmtcReader& r) { r.onAction(action); });
}

void SmtcReader::post(std::function<void(SmtcReader&)> work)
{
    queue_.TryEnqueue([self = shared_from_this(), work = std::move(work)] {
        if (!self->stopped_) self->guarded(work);
    });
}

void SmtcReader::guarded(const std::function<void(SmtcReader&)>& work)
{
    // A session can vanish while we read it (the app closed): its calls then
    // throw. Uncaught in a queued task, that would end the app; the next
    // SessionsChanged brings us back in sync.
    try {
        work(*this);
    } catch (winrt::hresult_error const& e) {
        platform::logLine("media: session went away mid-update (0x%08x)", static_cast<unsigned>(e.code()));
    } catch (std::exception const& e) {
        platform::logLine("media: error while reading (%s)", e.what());
    }
}

auto SmtcReader::onReaderThread(void (SmtcReader::*method)())
{
    return [weak = weak_from_this(), method](auto&&...) {
        if (auto self = weak.lock()) self->post([method](SmtcReader& r) { (r.*method)(); });
    };
}

winrt::fire_and_forget SmtcReader::connect()
{
    auto self = shared_from_this(); // held across the await
    Manager manager{ nullptr };
    std::optional<winrt::hresult> failed;
    try {
        manager = co_await Manager::RequestAsync();
    } catch (winrt::hresult_error const& e) {
        failed = e.code(); // maybe thrown on a pool thread: handled below, on the reader's thread
    }
    co_await onReader();
    if (failed) {
        platform::logLine("media: session manager unavailable (0x%08x)", static_cast<unsigned>(*failed));
        co_return;
    }
    if (stopped_) co_return;
    try {
        manager_ = manager;
        currentChanged_ = manager_.CurrentSessionChanged(winrt::auto_revoke, onReaderThread(&SmtcReader::pickSession));
        sessionsChanged_ = manager_.SessionsChanged(winrt::auto_revoke, onReaderThread(&SmtcReader::onSessionsChanged));
        platform::logLine("media: %u session(s)", manager_.GetSessions().Size());
        onSessionsChanged();
    } catch (winrt::hresult_error const& e) {
        platform::logLine("media: couldn't watch the sessions (0x%08x)", static_cast<unsigned>(e.code()));
    }
}

void SmtcReader::disconnect()
{
    try {
        currentChanged_.revoke();
        sessionsChanged_.revoke();
        propertiesChanged_.revoke();
        timelineChanged_.revoke();
        shownPlaybackChanged_.revoke();
        playbackWatchers_.clear();
    } catch (winrt::hresult_error const&) {
        // Windows' side already gone: nothing left to unsubscribe from.
    }
    manager_ = nullptr;
    session_ = nullptr;
    chosen_ = nullptr;
}

void SmtcReader::onSessionsChanged()
{
    watchAllSessions();
    pickSession();
}

void SmtcReader::watchAllSessions()
{
    playbackWatchers_.clear(); // revokes the old subscriptions
    swappedApp_.clear();       // new objects: a second one is worth a line again
    for (auto const& s : manager_.GetSessions()) {
        winrt::hstring id = s.SourceAppUserModelId();
        auto changed = [weak = weak_from_this(), name = appName(id)](auto&&...) {
            if (auto self = weak.lock()) self->post([name](SmtcReader& r) { r.onPlaybackChanged(name); });
        };
        playbackWatchers_.push_back({ s, winrt::to_string(id), s.PlaybackInfoChanged(winrt::auto_revoke, changed) });
    }
}

void SmtcReader::onPlaybackChanged(const std::string& app)
{
    // Play/pause from any app: should show on the island at once. Named, so a
    // late or stale state can be told apart from another app's (decision 181).
    platform::logLine("media: playback changed (%s)", app.c_str());
    showPlaybackNow();
    pickSession();
    recheckSoon();
}

void SmtcReader::onTimelineChanged()
{
    showPlaybackNow();
    refresh();
}

// Play/pause, the bar, shuffle and repeat read synchronously and shown at
// once, over the last full read (decision 116). The properties' read can take
// seconds (Spotify); a pause doesn't wait for it. The full read still follows.
void SmtcReader::showPlaybackNow()
{
    if (!session_) return;
    try {
        NowPlaying playback = readPlayback(session_.GetPlaybackInfo(), session_.GetTimelineProperties(), out_.now());
        if (shownPlaying_ != playback.timeline.playing) {
            if (shownPlaying_) platform::logLine("media: %s (from the playback event)", playback.timeline.playing ? "playing" : "paused");
            shownPlaying_ = playback.timeline.playing;
        }
        out_.showPlayback(winrt::to_string(session_.SourceAppUserModelId()), playback);
    } catch (winrt::hresult_error const&) {
        // Mid-switch or closing: the full read handles it (and retries).
    }
}

winrt::fire_and_forget SmtcReader::recheckSoon()
{
    std::weak_ptr<SmtcReader> weak = weak_from_this();
    co_await winrt::resume_after(tokens_.mediaSessionRecheck);
    if (auto self = weak.lock()) self->post([](SmtcReader& r) { r.pickSession(); });
}

void SmtcReader::pickSession()
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
    chosen = watchedFor(chosen); // not a second object whose playback stays as it was (decision 181)

    if (chosen != session_) {
        session_ = chosen;
        propertiesChanged_.revoke();
        timelineChanged_.revoke();
        shownPlaybackChanged_.revoke();
        if (session_) {
            propertiesChanged_ = session_.MediaPropertiesChanged(winrt::auto_revoke, onReaderThread(&SmtcReader::onPropertiesChanged));
            timelineChanged_ = session_.TimelinePropertiesChanged(winrt::auto_revoke, onReaderThread(&SmtcReader::onTimelineChanged));
            // Every session's playback is watched in watchAllSessions. One that
            // isn't among them is watched here: what's read must raise its own events.
            bool watched = std::any_of(playbackWatchers_.begin(), playbackWatchers_.end(), [&](auto const& w) { return w.session == session_; });
            if (!watched)
                shownPlaybackChanged_ = session_.PlaybackInfoChanged(winrt::auto_revoke, onReaderThread(&SmtcReader::showPlaybackNow));
            platform::logLine("media: showing %s", appName(session_.SourceAppUserModelId()).c_str());
        }
        propertiesDirty_ = true; // another session: read its picture fresh
    }
    refresh(); // also when only the session count changed (source button)
}

void SmtcReader::refresh()
{
    if (loading_ && out_.now() - loadStarted_ < tokens_.mediaReadStuck) {
        reloadWanted_ = true;
        // If this read hangs, read again when it counts as stuck, not at the
        // next event, which can be 10 s away (decision 116).
        if (!stuckCheckQueued_) {
            stuckCheckQueued_ = true;
            refreshLater(loadStarted_ + tokens_.mediaReadStuck - out_.now() + Ms{ 1 }, false);
        }
        return;
    }
    stuckCheckQueued_ = false;
    if (loading_) platform::logLine("media: a read is stuck, reading again");
    load();
}

void SmtcReader::loaded()
{
    loading_ = false;
    if (std::exchange(reloadWanted_, false)) refresh();
}

void SmtcReader::onPropertiesChanged()
{
    propertiesDirty_ = true;
    refresh();
}

winrt::fire_and_forget SmtcReader::refreshLater(Ms delay, bool rereadArt)
{
    std::weak_ptr<SmtcReader> weak = weak_from_this();
    co_await winrt::resume_after(std::max(delay, Ms{ 0 }));
    if (auto self = weak.lock())
        self->post([rereadArt](SmtcReader& r) {
            if (rereadArt) r.propertiesDirty_ = true;
            r.refresh();
        });
}

void SmtcReader::recheckArtIfMissing()
{
    if (artRecheckQueued_) return;
    auto attempt = art_.wantsRecheck(tokens_.mediaArtRechecks);
    if (!attempt) return;
    artRecheckQueued_ = true;
    recheckArt(tokens_.mediaArtRecheck * static_cast<int>(*attempt));
}

winrt::fire_and_forget SmtcReader::recheckArt(Ms delay)
{
    std::weak_ptr<SmtcReader> weak = weak_from_this();
    co_await winrt::resume_after(std::max(delay, Ms{ 0 }));
    auto self = weak.lock();
    if (!self) co_return;
    self->post([](SmtcReader& r) {
        r.artRecheckQueued_ = false;
        if (r.art_.art()) return; // it came meanwhile
        platform::logLine("media: still no cover, reading again");
        r.propertiesDirty_ = true; // decode whatever picture the read finds
        r.refresh();
    });
}

void SmtcReader::onArtDecoded(std::uint64_t id, std::shared_ptr<const ImageData> image)
{
    // The island's colour can only follow a new track once its cover is
    // read: the gap between this line and "new track" is a late recolour's wait.
    bool failed = !image;
    if (art_.onDecoded(id, std::move(image))) {
        platform::logLine("media: cover shown");
        refresh();
    } else if (failed) {
        recheckArtIfMissing(); // Spotify's picture can be unreadable for a moment
    }
}

} // namespace cup
