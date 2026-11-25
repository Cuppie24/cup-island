#pragma once
// Windows' media sessions (SMTC, TR §6), read on a thread of their own
// (decision 144). Every call into a session or the session manager is a call
// into another process, and one never returned after a wake: made on the UI
// thread, it froze the island until it was killed. Here a hung call only
// stops the media updating.
//
// Threading: everything below `private:` runs on the reader's thread (a
// DispatcherQueue's). WinRT events, timers and cover decodes come back to it
// through post(), whatever thread Windows used (decision 060). It never
// touches the Host: it reports through Out, which the provider posts to the
// UI thread. Lambdas and coroutines hold the reader by shared or weak
// pointer, so it may outlive the provider that started it.
#include "core/provider.h"
#include "core/tokens.h"
#include "providers/media/art_state.h"
#include "providers/media/media_presenter.h"
#include "providers/media/session_choice.h"
#include "providers/media/smtc_reading.h"

#include <winrt/Windows.System.h>

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace cup {

class SmtcReader : public std::enable_shared_from_this<SmtcReader> {
public:
    // Called on the reader's thread: each must hand its work to the UI thread.
    struct Out {
        std::function<Ms()> now;                                                 // the Host's clock, safe from any thread
        std::function<void(std::optional<MediaSnapshot>)> show;                  // a full read; nullopt: nothing plays
        std::function<void(std::string sessionKey, NowPlaying playback)> showPlayback; // play/pause at once (decision 116)
    };

    // From the UI thread. `settings`: the tokens, copied here, since tokens()
    // is the UI thread's and a reload rewrites it.
    static std::shared_ptr<SmtcReader> start(Out out, const Tokens& settings);
    void stop();                    // returns at once; late events, timers and reads become no-ops
    void act(const Action& action); // a media button (not "open the app": the provider does that)

private:
    using Manager = wmc::GlobalSystemMediaTransportControlsSessionManager;
    using Session = wmc::GlobalSystemMediaTransportControlsSession;
    using Properties = wmc::GlobalSystemMediaTransportControlsSessionMediaProperties;
    using PlaybackInfo = wmc::GlobalSystemMediaTransportControlsSessionPlaybackInfo;

    SmtcReader(Out out, const Tokens& settings) : out_(std::move(out)), tokens_(settings) {}

    // Runs `work` on the reader's thread unless stopped; a session's error is logged, not fatal.
    void post(std::function<void(SmtcReader&)> work);
    void guarded(const std::function<void(SmtcReader&)>& work);
    auto onReaderThread(void (SmtcReader::*method)()); // an event handler that posts `method`

    winrt::fire_and_forget connect();
    void disconnect(); // stop()'s revokes: calls into Windows too, so made here
    void onSessionsChanged();
    void onPlaybackChanged(const std::string& app); // any app started/paused/stopped; `app` named in the log
    void onTimelineChanged(); // the shown session's position or state
    void showPlaybackNow();   // its playback read synchronously and shown, ahead of the full read (decision 116)
    void logStatus(Session const& session, PlaybackInfo const& info); // Windows' own status, when it's neither playing nor paused (decision 182)
    void watchAllSessions();
    winrt::fire_and_forget recheckSoon();
    void pickSession();         // which session to show: manual choice, else Windows' current one
    Session watchedFor(Session const& session); // the watched object to read for it (decision 181)
    void refresh();             // re-read the shown session
    void onPropertiesChanged(); // the app re-sent title/art: the picture may be new
    // Re-read after `delay`; `rereadArt` treats it as a properties update (decode the cover again).
    winrt::fire_and_forget refreshLater(Ms delay, bool rereadArt);
    // A cover still missing: re-read the session in a while (ArtState::wantsRecheck).
    void recheckArtIfMissing();
    winrt::fire_and_forget recheckArt(Ms delay);
    void onArtDecoded(std::uint64_t id, std::shared_ptr<const ImageData> image);
    winrt::fire_and_forget load(); // one at a time: see loading_
    // The rest of a read, once the properties are in: may throw, so load() calls it inside its try.
    void showRead(Session const& session, Properties const& properties, bool propertiesChanged, Ms began);
    void loaded();                 // a read ended: start the one asked for meanwhile, if any
    void onAction(const Action& action);
    void cycleSource();
    std::vector<SessionView> describe(Session const& remembered) const; // the current sessions, for session_choice

    // co_await onReader(): back on the reader's thread, through its own queue
    // (decision 060: never trust where a WinRT co_await resumes).
    struct OnReader {
        winrt::Windows::System::DispatcherQueue queue;
        std::thread::id thread;
        bool await_ready() const noexcept { return std::this_thread::get_id() == thread; }
        void await_suspend(std::coroutine_handle<> resume) const { queue.TryEnqueue([resume] { resume.resume(); }); }
        void await_resume() const noexcept {}
    };
    OnReader onReader() const { return { queue_, thread_ }; }

    Out out_;
    Tokens tokens_;
    std::atomic<bool> stopped_ = false;
    winrt::Windows::System::DispatcherQueueController controller_{ nullptr };
    winrt::Windows::System::DispatcherQueue queue_{ nullptr };
    std::thread::id thread_; // the queue's thread, set by its first task

    Manager manager_{ nullptr };
    Session session_{ nullptr };
    Session chosen_{ nullptr }; // manual pick via the source button; null = follow Windows
    std::string chosenApp_;     // its app id, to find it again if Windows hands out a new object
    std::string mainApp_;       // app of Windows' main media (what the media keys control), last seen
    std::string swappedApp_;    // the app whose second object was last swapped for its watched one: logged once (decision 181)

    // One read at a time; events during it ask for one more after it. Not
    // "newest wins": Spotify sends timeline updates faster than a read takes
    // around a pause or skip, and each dropped the read before it, so nothing
    // showed until it went quiet (seconds; decision 005).
    bool loading_ = false;
    Ms loadStarted_{};  // a read older than mediaReadStuck doesn't hold the next back
    bool reloadWanted_ = false;
    bool stuckCheckQueued_ = false;    // a re-read queued for when the read in flight counts as stuck (decision 116)
    std::optional<bool> shownPlaying_; // logged when it changes, to time pauses
    std::optional<PlaybackStatus> loggedStatus_; // the shown session's own, last logged (decision 182)
    std::string loggedTrack_;          // the last track logged: a recolour's wait starts with its line
    ArtState art_;                     // the cover: when to decode, when to hold a new track back
    bool propertiesDirty_ = false;     // properties re-sent since the last completed read
    bool artRecheckQueued_ = false;    // one re-read for a missing cover at a time
    std::size_t readFailures_ = 0;     // in a row; retried up to mediaReadRetries

    Manager::CurrentSessionChanged_revoker currentChanged_;
    Manager::SessionsChanged_revoker sessionsChanged_;
    Session::MediaPropertiesChanged_revoker propertiesChanged_;
    Session::TimelinePropertiesChanged_revoker timelineChanged_;
    // The shown session's own playback, when it isn't a watched object (an
    // app with two sessions): whatever is read raises its own events (decision 181).
    Session::PlaybackInfoChanged_revoker shownPlaybackChanged_;
    // Every session's playback: Windows doesn't reliably raise
    // CurrentSessionChanged when another app starts playing. Each keeps its
    // session alive: a revoker only holds a weak reference, and a session
    // object nothing holds is freed and raises nothing (a pause then showed
    // only at the app's next timeline update, seconds later; decision 005).
    // These are also the objects read (watchedFor): Windows keeps them up to date.
    struct PlaybackWatcher {
        Session session{ nullptr };
        std::string app; // its SourceAppUserModelId, read once
        Session::PlaybackInfoChanged_revoker changed;
    };
    std::vector<PlaybackWatcher> playbackWatchers_;
};

} // namespace cup
