#include "providers/media/smtc_media_provider.h"
#include "providers/media/app_activation.h"

#include <chrono>
#include <string>
#include <utility>

namespace cup {

void SmtcMediaProvider::start(Host& host)
{
    presenter_ = std::make_unique<MediaPresenter>(host);
    presenter_->start();
    alive_ = std::make_shared<bool>(true);

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
                shown_ = snapshot;
                presenter_->show(snapshot);
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
    shown_.reset();
    presenter_->stop();
    presenter_.reset();
}

void SmtcMediaProvider::onAction(const Action& action)
{
    // Opening the app hands over the foreground, which only this click's
    // thread may do (app_activation.h): here, from what the last read showed.
    if (action.actionId == media_action::kOpenSource) {
        if (shown_)
            openSourceApp(std::wstring(winrt::to_hstring(shown_->sessionKey)), std::wstring(winrt::to_hstring(shown_->media.title)));
        return;
    }
    if (reader_) reader_->act(action);
}

} // namespace cup
