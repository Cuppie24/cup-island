// MediaPresenter: which Host calls each "what's playing" snapshot produces,
// and the media widget on Home (decision 019).

#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/media/art_state.h"
#include "providers/media/media_presenter.h"
#include "providers/media/now_playing.h"
#include "providers/media/session_choice.h"
#include "providers/media/tab_match.h"

#include <vector>

namespace cup::test {
namespace {

struct FakeHost : Host {
    Ms clock{ 0 };
    std::vector<Activity> started;
    std::vector<Alert> alerts;
    std::vector<std::string> ended;
    std::vector<Panel> panels;
    std::vector<std::string> removedPanels;

    void startActivity(Activity a) override { started.push_back(std::move(a)); }
    void endActivity(const std::string& id) override { ended.push_back(id); }
    void postAlert(Alert a) override { alerts.push_back(std::move(a)); }
    Ms now() const override { return clock; }
    void publishPanel(Panel p) override { panels.push_back(std::move(p)); }
    void removePanel(const std::string& id) override { removedPanels.push_back(id); }
    void wakeAt(std::optional<Ms>) override {}
    void ask(Dialog) override {}
    void reveal(const std::string&, const std::string&, std::size_t) override {}
    std::string load(const std::string&) const override { return {}; }
    void save(const std::string&, const std::string&) override {}
    Ms wallClock() const override { return clock; }
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
    const Slot& bubble = host.started[0].content.minimal;
    expect(bubble.size() == 1 && bubble[0].items.size() == 1 && bubble[0].items[0] == Item{ Waveform{ true } },
           "bubble (minimal) shows the equalizer bars while playing");
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
    // The widget: there from the start, even with nothing playing; a card to open, no buttons.
    FakeHost widgetHost;
    MediaPresenter widget(widgetHost);
    widget.start();
    expect(widgetHost.panels.size() == 1 && widgetHost.panels[0].id == MediaPresenter::kActivityId && widgetHost.panels[0].wide,
           "start publishes a wide media widget, under the activity's id");
    const Panel& idlePanel = widgetHost.panels[0];
    expect(hasCard(idlePanel.content) && std::get<Text>(idlePanel.content.tile[0].items[0]).value == "Not playing" &&
           std::get<Icon>(idlePanel.content.tileLeading[0].items[0]).glyph == Glyph::Music, "nothing playing: says so, with a card");
    widget.show(track("Spotify.exe", "One"));
    widget.stop();
    expect(widgetHost.ended.size() == 1 && widgetHost.removedPanels == std::vector<std::string>{ MediaPresenter::kActivityId },
           "stop ends what plays and removes the widget");

    Content playing = nowPlayingContent({ .title = "Song", .artist = "Band", .art = std::make_shared<ImageData>(),
                                          .timeline = { .duration = Ms{ 1000 }, .playing = true } });
    expect(std::holds_alternative<Image>(playing.tileLeading[0].items[0]), "playing: the art on the left");
    expect(playing.tile[0].items[0] == Item{ Text{ .value = "Song", .style = TextStyle::Title, .marquee = true } } &&
           playing.tile[0].trailing[0] == Item{ Waveform{ true } }, "the title swims, the bars beside it");
    expect(std::get<Text>(playing.tile[1].items[0]).value == "Band", "the artist under it");

    // On Home: the activity's widget replaces the idle one in place, and back.
    IslandState s = reduce(IslandState{}, PanelPublished{ idlePanel }, Ms{ 0 });
    s = reduce(s, PillClicked{}, Ms{ 0 });
    auto titleOnHome = [](const IslandState& state) {
        for (const Placed& p : layout(state, Ms{ 0 }).elements)
            if (p.key == std::string("tile/") + MediaPresenter::kActivityId + "/0/0") return std::get<Text>(p.item).value;
        return std::string("none");
    };
    expect(titleOnHome(s) == "Not playing", "Home: the idle media widget");
    IslandState live = reduce(s, ActivityStarted{ { .id = MediaPresenter::kActivityId, .content = playing } }, Ms{ 0 });
    live = reduce(live, HomeOpened{}, Ms{ 0 });
    expect(titleOnHome(live) == "Song", "playing: the widget shows the track");
    Scene liveHome = layout(live, Ms{ 0 });
    expect(liveHome.tiles.size() == 1 && liveHome.tiles[0].opens, "a click opens the player's card");
    expect(titleOnHome(reduce(live, ActivityEnded{ MediaPresenter::kActivityId }, Ms{ 0 })) == "Not playing", "ended: idle again");

    expect(nextRepeat(Repeat::Off) == Repeat::All && nextRepeat(Repeat::All) == Repeat::One && nextRepeat(Repeat::One) == Repeat::Off,
           "repeat cycles off -> all -> one");

    // Session choice (decision 005): the source button's pick must survive Windows
    // handing out a new object for the same session.
    std::vector<SessionView> sessions = { { "Chrome" }, { "Spotify.exe" }, { "Chrome" } };
    expect(!findSession(sessions, "VLC"), "app gone: pick is lost");
    expect(findSession(sessions, "Spotify.exe") == 1u, "new object, same app: found by app id");
    sessions[2].remembered = true;
    expect(findSession(sessions, "Chrome") == 2u, "same object wins over the first tab of the same app");
    expect(!mainMediaMoved("Spotify.exe", "Spotify.exe"), "same main app (even a new object): no move");
    expect(!mainMediaMoved("Spotify.exe", ""), "a moment without main media: no move, the pick stays");
    expect(mainMediaMoved("Spotify.exe", "Chrome") && mainMediaMoved("", "Spotify.exe"), "another app took over: move");
    expect(nextSession(3, 1u) == 2 && nextSession(3, 2u) == 0, "source button steps and wraps");
    expect(nextSession(3, std::nullopt) == 0 && nextSession(0, std::nullopt) == 0, "nothing shown: start at the first");

    // The expanded art opens the playing app, when the provider can; nowhere else is the art clickable.
    Content openable = nowPlayingContent({ .title = "T", .canOpenSource = true });
    expect(std::get<Image>(openable.expandedLeading[0].items[0]).actionId == media_action::kOpenSource, "expanded art opens the app");
    expect(std::get<Image>(openable.compactLeading[0].items[0]).actionId.empty() &&
           std::get<Image>(openable.alertLeading[0].items[0]).actionId.empty(), "compact/alert art isn't (a click there expands)");
    expect(std::get<Image>(nowPlayingContent({ .title = "T" }).expandedLeading[0].items[0]).actionId.empty(),
           "a provider that can't open its app: plain art");

    // Finding the browser tab that plays the media, from tab names.
    std::vector<std::string> tabs = { "Inbox (3) - Gmail", "Lo-fi beats - YouTube", "lo-fi beats - YouTube - Audio playing" };
    expect(findMediaTab(tabs, "Lo-fi Beats") == 2u, "any case matches; the tab marked as playing audio wins");
    tabs.pop_back();
    expect(findMediaTab(tabs, "Lo-fi beats") == 1u, "no audio marker: the first tab with the title");
    expect(!findMediaTab(tabs, "Something else") && !findMediaTab(tabs, "") && !findMediaTab(tabs, "a"),
           "no match, or a title too short to trust: nothing");

    // Cover art (decisions 005, 056): when to decode. A track never waits for its cover.
    auto pixel = [](std::uint8_t shade) {
        auto image = std::make_shared<ImageData>();
        image->width = image->height = 1;
        image->bgra = { shade, shade, shade, 255 };
        return std::shared_ptr<const ImageData>(image);
    };
    ArtState covers;
    auto first = covers.onRead("Chrome|Song A", true, true);
    expect(first && !covers.art(), "new track with a thumbnail: decode it, the track shows meanwhile without a cover");
    expect(covers.onDecoded(*first, pixel(10)) && covers.art(), "decoded: the cover comes in");

    // Chrome, two tabs: the title switches first, the picture in a later update.
    auto stale = covers.onRead("Chrome|Song B", true, true);
    expect(stale && !covers.art(), "title changed: the old cover is dropped at once");
    expect(covers.onDecoded(*stale, pixel(10)), "decoded (still the old tab's picture)");
    auto fresh = covers.onRead("Chrome|Song B", true, true);
    expect(fresh.has_value(), "same title, re-sent properties: decode again (the picture may be new)");
    expect(covers.onDecoded(*fresh, pixel(20)) && covers.art()->bgra[0] == 20, "the right picture replaces the stale one");
    expect(!covers.onRead("Chrome|Song B", true, false), "timeline/playback reads don't decode");
    auto same = covers.onRead("Chrome|Song B", true, true);
    expect(!covers.onDecoded(*same, pixel(20)), "identical pixels: nothing to update");

    // Out-of-order and failed decodes.
    auto older = covers.onRead("Chrome|Song B", true, true);
    auto newer = covers.onRead("Chrome|Song B", true, true);
    expect(!covers.onDecoded(*older, pixel(30)) && covers.art()->bgra[0] == 20, "a superseded decode is ignored");
    expect(!covers.onDecoded(*newer, nullptr) && covers.art(), "a failed re-decode keeps the cover we have");
    auto broken = covers.onRead("Chrome|Song C", true, true);
    expect(!covers.onDecoded(*broken, nullptr) && !covers.art(), "a failed first decode: the track stays without a cover");
    expect(!covers.onRead("Chrome|Song D", false, true) && !covers.art(), "no thumbnail: nothing to decode");
}

} // namespace cup::test
