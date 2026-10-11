// MediaPresenter: which Host calls each "what's playing" snapshot produces.

#include "core/tokens.h"
#include "expect.h"
#include "providers/media/media_presenter.h"
#include "providers/media/now_playing.h"

#include <vector>

namespace cup::test {
namespace {

struct FakeHost : Host {
    Ms clock{ 0 };
    std::vector<Activity> started;
    std::vector<Alert> alerts;
    std::vector<std::string> ended;

    void startActivity(Activity a) override { started.push_back(std::move(a)); }
    void endActivity(const std::string& id) override { ended.push_back(id); }
    void postAlert(Alert a) override { alerts.push_back(std::move(a)); }
    Ms now() const override { return clock; }
};

MediaSnapshot track(const char* app, const char* title, bool playing = true)
{
    return { app, { .title = title, .artist = "Artist", .timeline = { .duration = Ms{ 180000 }, .playing = playing } } };
}

} // namespace

void runMediaTests()
{
    FakeHost host;
    MediaPresenter presenter(host);

    presenter.show(track("Spotify.exe", "One"));
    expect(host.started.size() == 1 && host.alerts.empty(), "first snapshot shows without an alert");
    expect(host.started[0].id == MediaPresenter::kActivityId && !host.started[0].expiresAt, "playing: no expiry");

    presenter.show(track("Spotify.exe", "One"));
    expect(host.started.size() == 1, "identical snapshot sends nothing");

    auto withArt = track("Spotify.exe", "One");
    withArt.media.art = std::make_shared<ImageData>();
    withArt.media.timeline.position = Ms{ 5000 };
    presenter.show(withArt);
    expect(host.started.size() == 2 && host.alerts.empty(), "same track, new art/position: update, no alert");

    // Timeline re-sent with a slightly different anchor (same real position): no republish.
    host.clock = Ms{ 3000 };
    auto resent = withArt;
    resent.media.timeline.position = Ms{ 8200 }; // 5000 + 3000 elapsed, 200 ms of noise
    resent.media.timeline.anchoredAt = Ms{ 3000 };
    presenter.show(resent);
    expect(host.started.size() == 2, "timeline re-sent within tolerance: nothing sent");
    auto jumped = resent;
    jumped.media.timeline.position = Ms{ 60000 };
    presenter.show(jumped);
    expect(host.started.size() == 3, "a real jump (seek) is published");

    presenter.show(track("Spotify.exe", "Two"));
    expect(host.alerts.size() == 1 && host.alerts[0].coalesceKey == "track" && host.alerts[0].activityId == MediaPresenter::kActivityId,
           "track change in the same app alerts once, coalesced");
    expect(host.alerts[0].content.label == "Two - Artist", "alert shows the new track");

    presenter.show(track("Chrome", "Video"));
    expect(host.alerts.size() == 1, "switching to another app doesn't alert");

    // Pausing: expiry fixed at the first paused snapshot.
    host.clock = Ms{ 10000 };
    presenter.show(track("Chrome", "Video", false));
    Ms expected = Ms{ 10000 } + tokens().mediaPausedLinger;
    expect(host.started.back().expiresAt == expected, "pause sets the linger expiry");
    host.clock = Ms{ 20000 };
    auto seeked = track("Chrome", "Video", false);
    seeked.media.timeline.position = Ms{ 30000 };
    presenter.show(seeked);
    expect(host.started.back().expiresAt == expected, "later paused snapshots don't extend it");
    presenter.show(track("Chrome", "Video", true));
    expect(!host.started.back().expiresAt, "resuming clears the expiry");

    presenter.show(std::nullopt);
    expect(host.ended.size() == 1 && host.ended[0] == MediaPresenter::kActivityId, "no session ends the activity");
    presenter.show(std::nullopt);
    expect(host.ended.size() == 1, "ending twice sends nothing");

    presenter.show(track("Chrome", "Video 2"));
    expect(host.alerts.size() == 1, "a session reappearing doesn't alert");

    auto untitled = track("Chrome", "");
    presenter.show(untitled);
    expect(host.alerts.size() == 1, "no alert for an empty title");

    // Shuffle/repeat buttons: only for apps that support them, lit when on.
    auto controlsOf = [](const NowPlaying& m) { return nowPlayingContent(m).expandedBottom.back(); };
    Row plain = controlsOf({ .title = "T" });
    expect(plain.leading.empty() && plain.trailing.empty(), "no shuffle/repeat when the app doesn't support them");
    Row toggles = controlsOf({ .title = "T", .canShuffle = true, .shuffle = true, .canRepeat = true, .repeat = Repeat::One,
                               .showSourceSwitch = true });
    auto* shuffleButton = toggles.leading.size() == 1 ? std::get_if<Button>(&toggles.leading[0]) : nullptr;
    expect(shuffleButton && shuffleButton->glyph == Glyph::Shuffle && shuffleButton->on == true, "shuffle on the left, lit");
    auto* repeatButton = toggles.trailing.size() == 1 ? std::get_if<Button>(&toggles.trailing[0]) : nullptr;
    expect(repeatButton && repeatButton->glyph == Glyph::RepeatOne && repeatButton->on == true,
           "repeat-one alone in the bottom-right corner");

    // The source switch lives in the top-right corner, not with the playback controls.
    Content withSwitch = nowPlayingContent({ .title = "T", .showSourceSwitch = true });
    auto* sourceButton = withSwitch.expandedTrailing.size() == 1 && withSwitch.expandedTrailing[0].items.size() == 1
                             ? std::get_if<Button>(&withSwitch.expandedTrailing[0].items[0]) : nullptr;
    expect(sourceButton && sourceButton->actionId == media_action::kSource && !sourceButton->on, "source switch in expanded.trailing");
    expect(nowPlayingContent({ .title = "T" }).expandedTrailing.empty(), "one media app: no corner button");

    // Track N of M, after the app name in the caption.
    expect(trackPosition(3, 12) == "Track 3 of 12" && trackPosition(3, 0) == "Track 3" && trackPosition(3, 2) == "Track 3" &&
           trackPosition(0, 12).empty(), "track position text, tolerant of missing or bad counts");
    Content numbered = nowPlayingContent({ .title = "T", .source = "Spotify", .trackNumber = 3, .trackCount = 12 });
    auto* caption = std::get_if<Text>(&numbered.expandedCenter.back().items[0]);
    expect(caption && caption->value == "Spotify \u00B7 Track 3 of 12" && caption->style == TextStyle::Caption,
           "caption: app \u00B7 track position");
    Content unnumbered = nowPlayingContent({ .title = "T", .source = "Chrome" });
    expect(std::get<Text>(unnumbered.expandedCenter.back().items[0]).value == "Chrome", "no position: just the app");
    expect(nextRepeat(Repeat::Off) == Repeat::All && nextRepeat(Repeat::All) == Repeat::One && nextRepeat(Repeat::One) == Repeat::Off,
           "repeat cycles off -> all -> one");
}

} // namespace cup::test
