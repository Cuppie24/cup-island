#pragma once
// Shows whatever Windows is playing (Spotify, browsers, VLC, Media Player…)
// through the public media session API (SMTC, TR §6). Event-driven: it reads
// a session only when Windows says something changed.
//
// Threading: WinRT raises events on background threads; every handler just
// posts to the UI thread. Everything else runs on the UI thread. Coroutines
// return to it through post_ after every WinRT call (onUi): Windows usually
// resumes them there itself, but not when it can't switch back (after sleep),
// and then they ran on a pool thread and froze the island (decision 060).

#include "core/provider.h"
#include "providers/media/art_state.h"
#include "providers/media/media_presenter.h"
#include "providers/media/session_choice.h"
#include "providers/media/smtc_reading.h"

#include <coroutine>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace cup {

class SmtcMediaProvider : public Provider {
public:
    using PostToUi = std::function<void(std::function<void()>)>;
    explicit SmtcMediaProvider(PostToUi postToUi) : post_(std::move(postToUi)) {}

    const char* id() const override { return "media"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action& action) override;

private:
    using Manager = wmc::GlobalSystemMediaTransportControlsSessionManager;
    using Session = wmc::GlobalSystemMediaTransportControlsSession;

    winrt::fire_and_forget connect(std::shared_ptr<bool> alive);
    void onSessionsChanged();
    void onPlaybackChanged(); // any app started/paused/stopped
    void watchAllSessions();
    winrt::fire_and_forget recheckSoon(std::shared_ptr<bool> alive);
    void pickSession();   // which session to show: manual choice, else Windows' current one
    void refresh();       // re-read the shown session
    void onPropertiesChanged(); // the app re-sent title/art: the picture may be new
    // Re-read after `delay`; `rereadArt` treats it as a properties update (decode the cover again).
    winrt::fire_and_forget refreshLater(Ms delay, bool rereadArt, std::shared_ptr<bool> alive);
    winrt::fire_and_forget load(std::shared_ptr<bool> alive); // one at a time: see loading_
    void loaded(); // a read ended: start the one asked for meanwhile, if any
    void cycleSource();
    std::vector<SessionView> describe(Session const& remembered) const; // the current sessions, for session_choice
    auto onUiThread(void (SmtcMediaProvider::*method)()); // event handler that posts `method`
    void guarded(void (SmtcMediaProvider::*method)());    // runs it; a vanished session's error is logged, not fatal

    // co_await onUi(): back on the UI thread, through our own queue.
    struct OnUi {
        PostToUi post;
        std::thread::id ui;
        bool await_ready() const noexcept { return std::this_thread::get_id() == ui; }
        void await_suspend(std::coroutine_handle<> resume) const { post([resume] { resume.resume(); }); }
        void await_resume() const noexcept {}
    };
    OnUi onUi() const { return { post_, uiThread_ }; }

    PostToUi post_;
    std::thread::id uiThread_; // start()'s thread
    Host* host_ = nullptr;
    std::unique_ptr<MediaPresenter> presenter_;
    std::shared_ptr<bool> alive_; // false after stop(): late callbacks become no-ops

    Manager manager_{ nullptr };
    Session session_{ nullptr };
    Session chosen_{ nullptr };  // manual pick via the source button; null = follow Windows
    std::string chosenApp_;      // its app id, to find it again if Windows hands out a new object
    std::string mainApp_;
    winrt::hstring shownTitle_; // the title on screen: picks the right browser window when opening the app        // app of Windows' main media (what the media keys control), last seen
    // One read at a time; events during it ask for one more after it. Not
    // "newest wins": Spotify sends timeline updates faster than a read takes
    // around a pause or skip, and each dropped the read before it, so nothing
    // showed until it went quiet (seconds; decision 005).
    bool loading_ = false;
    Ms loadStarted_{};  // a read older than mediaReadStuck doesn't hold the next back
    bool reloadWanted_ = false;
    std::optional<bool> shownPlaying_; // logged when it changes, to time pauses

    std::string loggedTrack_; // the last track logged: a recolour's wait starts with its line
    ArtState art_;                // the cover: when to decode, when to hold a new track back
    bool propertiesDirty_ = false; // properties re-sent since the last completed read
    std::size_t readFailures_ = 0; // in a row; retried up to mediaReadRetries

    Manager::CurrentSessionChanged_revoker currentChanged_;
    Manager::SessionsChanged_revoker sessionsChanged_;
    Session::MediaPropertiesChanged_revoker propertiesChanged_;
    Session::TimelinePropertiesChanged_revoker timelineChanged_;
    // Every session's playback: Windows doesn't reliably raise
    // CurrentSessionChanged when another app starts playing. Each keeps its
    // session alive: a revoker only holds a weak reference, and a session
    // object nothing holds is freed and raises nothing (a pause then showed
    // only at the app's next timeline update, seconds later; decision 005).
    struct PlaybackWatcher {
        Session session{ nullptr };
        Session::PlaybackInfoChanged_revoker changed;
    };
    std::vector<PlaybackWatcher> playbackWatchers_;
};

} // namespace cup
