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
#include "core/provider.h"
#include "providers/media/media_presenter.h"
#include "providers/media/smtc_reader.h"

#include <functional>
#include <memory>
#include <optional>

namespace cup {

class SmtcMediaProvider : public Provider {
public:
    using PostToUi = std::function<void(std::function<void()>)>;
    explicit SmtcMediaProvider(PostToUi postToUi) : post_(std::move(postToUi)) {}

    const char* id() const override { return "media"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action& action) override;
    void onWake() override { presenter_->onWake(); } // a vanished session's grace is over (decision 074)

private:
    PostToUi post_;
    std::unique_ptr<MediaPresenter> presenter_;
    std::shared_ptr<bool> alive_; // false after stop(): the reader's late reports become no-ops
    std::shared_ptr<SmtcReader> reader_;
    std::optional<MediaSnapshot> shown_; // the last full read: which app (and which tab) the art's click opens
};

} // namespace cup
