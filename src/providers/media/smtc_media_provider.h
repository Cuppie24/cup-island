#pragma once
// Shows whatever Windows is playing (Spotify, browsers, VLC, Media Player…)
// through the public media session API (SMTC, TR §6). Event-driven: it reads
// a session only when Windows says something changed.
//
// Threading: this class is the UI thread's side: the presenter (every
// decision, decision 005) and opening the app on a click. Every call into
// Windows' sessions is SmtcReader's, on a thread of its own: after a wake
// one such call never returned, and on the UI thread it froze the island
// (decision 144). What the reader reads is posted back here.
// The same for the playing app's volume (AppVolume, decision 180). Lyrics
// are asked for from here: an async request, answered on this thread.
#include "providers/media/app_volume.h" // first: it brings windows.h in before WinRT
#include "core/provider.h"
#include "providers/media/media_presenter.h"
#include "providers/media/smtc_reader.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace cup {

class SmtcMediaProvider : public Provider {
public:
    using PostToUi = std::function<void(std::function<void()>)>;
    // `appVersion`: named to the lyrics service, as to GitHub (decision 180).
    SmtcMediaProvider(PostToUi postToUi, std::string appVersion) : post_(std::move(postToUi)), appVersion_(std::move(appVersion)) {}

    const char* id() const override { return "media"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action& action) override;
    void onWake() override { presenter_->onWake(); } // a vanished session's grace is over (decision 074)
    void onHover(const Action& what, bool over) override;
    void onCardClosed(const std::string&) override // the volume's bar is shut, the lyrics' page left: it opens as the card
    {
        presenter_->closeVolume();
        presenter_->showLyricsPage(false);
    }
    void onCardShown(const std::string& activityId) override; // its volume read again: the mixer may have moved it

private:
    PostToUi post_;
    std::unique_ptr<MediaPresenter> presenter_;
    std::shared_ptr<bool> alive_; // false after stop(): the reader's late reports become no-ops
    std::shared_ptr<SmtcReader> reader_;
    std::optional<MediaSnapshot> shown_; // the last full read: which app (and which tab) the art's click opens
    std::string appVersion_;
    std::shared_ptr<AppVolume> volume_;
    std::string levelOf_;                  // the app whose volume level_ is
    std::optional<AppVolume::Level> level_; // as last read or set here; none: that app has no volume to show
    void showLevel(const std::string& app, std::optional<AppVolume::Level> level);
};

} // namespace cup
