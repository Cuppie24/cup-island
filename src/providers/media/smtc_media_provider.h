#pragma once
// Shows whatever Windows is playing (Spotify, browsers, VLC, Media Player…)
// through the public media session API (SMTC, TR §6). Event-driven: it reads
// a session only when Windows says something changed.
//
// Threading: WinRT raises events on background threads; every handler just
// posts to the UI thread. Everything else, including coroutine continuations
// (they resume on the UI thread's apartment), runs on the UI thread.

#include "core/provider.h"
#include "providers/media/media_presenter.h"
#include "providers/media/smtc_reading.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
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
    winrt::fire_and_forget load(std::uint64_t generation, std::shared_ptr<bool> alive);
    void cycleSource();
    auto onUiThread(void (SmtcMediaProvider::*method)()); // event handler that posts `method`

    PostToUi post_;
    Host* host_ = nullptr;
    std::unique_ptr<MediaPresenter> presenter_;
    std::shared_ptr<bool> alive_; // false after stop(): late callbacks become no-ops

    Manager manager_{ nullptr };
    Session session_{ nullptr };
    Session chosen_{ nullptr };         // manual pick via the source button; null = follow Windows
    Session windowsCurrent_{ nullptr }; // Windows' main media (what the media keys control), last seen
    std::uint64_t generation_ = 0; // newer refresh wins; stale async reads are dropped

    std::string artKey_;   // app|title|artist|album|has-art the art belongs to
    bool artPending_ = false;
    std::shared_ptr<const ImageData> art_;

    Manager::CurrentSessionChanged_revoker currentChanged_;
    Manager::SessionsChanged_revoker sessionsChanged_;
    Session::MediaPropertiesChanged_revoker propertiesChanged_;
    Session::TimelinePropertiesChanged_revoker timelineChanged_;
    // Every session's playback: Windows doesn't reliably raise
    // CurrentSessionChanged when another app starts playing.
    std::vector<Session::PlaybackInfoChanged_revoker> playbackWatchers_;
};

} // namespace cup
