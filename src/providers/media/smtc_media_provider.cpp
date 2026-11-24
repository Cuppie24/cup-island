#include "providers/media/smtc_media_provider.h"
#include "providers/lyrics/lrclib_fetch.h"
#include "providers/media/app_activation.h"

#include <chrono>
#include <string>
#include <utility>

namespace cup {

void SmtcMediaProvider::start(Host& host)
{
    // Lyrics come from LRCLIB (decision 180); the presenter decides when to ask.
    presenter_ = std::make_unique<MediaPresenter>(host, [version = appVersion_](const LyricsQuery& query, LyricsLookup::Done done) {
        fetchLrclib(version, query, std::move(done));
    });
    presenter_->start();
    alive_ = std::make_shared<bool>(true);
    volume_ = AppVolume::start([this, post = post_, alive = alive_](std::string app, std::optional<AppVolume::Level> level) {
        post([this, alive, app = std::move(app), level] {
            if (*alive) showLevel(app, level);
        });
    });

    // The reader reports from its own thread: each report is posted here, and
    // dropped if the provider stopped meanwhile (alive_ is only read and
    // written on this thread).
    auto alive = alive_;
    SmtcReader::Out out{
        // The Host's clock, which the reader can't call from its thread: the
        // same steady clock, anchored to it once here.
        .now = [base = host.now(), anchor = std::chrono::steady_clock::now()] {
            return base + std::chrono::duration_cast<Ms>(std::chrono::steady_clock::now() - anchor);
        },
        .show = [this, post = post_, alive](std::optional<MediaSnapshot> snapshot) {
            post([this, alive, snapshot = std::move(snapshot)] {
                if (!*alive) return;
                // Another app, or another track: its volume read (again). Not on every
                // read: apps re-send their timelines often.
                bool another = snapshot && (!shown_ || shown_->sessionKey != snapshot->sessionKey || shown_->media.title != snapshot->media.title);
                shown_ = snapshot;
                presenter_->show(snapshot);
                if (another) volume_->read(snapshot->sessionKey);
            });
        },
        .showPlayback = [this, post = post_, alive](std::string sessionKey, NowPlaying playback) {
            post([this, alive, sessionKey = std::move(sessionKey), playback = std::move(playback)] {
                if (*alive) presenter_->showPlayback(sessionKey, playback);
            });
        },
    };
    reader_ = SmtcReader::start(std::move(out), tokens());
}

void SmtcMediaProvider::stop()
{
    *alive_ = false;
    reader_->stop(); // returns at once, even if its thread is stuck in a call into Windows
    reader_.reset();
    volume_->stop(); // the same
    volume_.reset();
    level_.reset();
    shown_.reset();
    presenter_->stop();
    presenter_.reset();
}

void SmtcMediaProvider::onAction(const Action& action)
{
    // Any control but the volume's own puts its bar away (decision 180).
    if (action.actionId != media_action::kVolume && action.actionId != media_action::kVolumeOpen) presenter_->closeVolume();
    // Opening the app hands over the foreground, which only this click's
    // thread may do (app_activation.h): here, from what the last read showed.
    if (action.actionId == media_action::kOpenSource) {
        if (shown_)
            openSourceApp(std::wstring(winrt::to_hstring(shown_->sessionKey)), std::wstring(winrt::to_hstring(shown_->media.title)));
        return;
    }
    if (action.actionId == media_action::kLyricsPage || action.actionId == media_action::kLyricsBack) {
        presenter_->showLyricsPage(action.actionId == media_action::kLyricsPage);
        return;
    }
    if (action.actionId == media_action::kLyrics) {
        presenter_->toggleLyrics();
        return;
    }
    // The speaker button (decision 180): the volume's bar shown or put away; shown, read afresh,
    // since Windows' mixer may have moved it.
    if (action.actionId == media_action::kVolumeOpen) {
        presenter_->toggleVolume();
        if (shown_ && volume_) volume_->read(shown_->sessionKey);
        return;
    }
    // The bar: shown at once as asked, then set on the volume's thread.
    if (action.actionId == media_action::kVolume) {
        if (!shown_ || !volume_ || !level_ || levelOf_ != shown_->sessionKey) return;
        presenter_->touchVolume(); // in use: it stays open
        volume_->set(levelOf_, action.value);
        showLevel(levelOf_, AppVolume::Level{ action.value, false });
        return;
    }
    if (reader_) reader_->act(action);
}

void SmtcMediaProvider::onCardShown(const std::string&)
{
    if (shown_ && volume_) volume_->read(shown_->sessionKey);
}

void SmtcMediaProvider::showLevel(const std::string& app, std::optional<AppVolume::Level> level)
{
    levelOf_ = app;
    level_ = level;
    presenter_->showVolume(app, level ? std::optional<float>(level->volume) : std::nullopt, level && level->muted);
}

} // namespace cup
