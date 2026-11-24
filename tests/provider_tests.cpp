// Providers end to end, without Windows: registry + demo provider + reduce()
// + layout() + hitTest(), the same loop the app runs.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/slot_layout.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/demo/demo_media_provider.h"
#include "providers/provider_registry.h"

#include <cmath>

namespace cup::test {
namespace {

struct Island {
    IslandState state;
    Ms now{ 0 };
    ProviderRegistry registry{ [this](const Event& e, const char*) { state = reduce(state, e, now); }, [this] { return now; } };

    Scene scene() const { return layout(state, now); }

    // Click the centre of the element with this key, like the app does.
    void click(const std::string& key)
    {
        for (const Placed& p : scene().elements) {
            if (p.key != key) continue;
            auto hit = hitTest(scene(), { p.frame.x + p.frame.width / 2, p.frame.y + p.frame.height / 2 });
            if (hit) registry.route(hit->action);
            else state = reduce(state, PillClicked{}, now);
            return;
        }
        expect(false, "no element " + key);
    }

    const Button* button(const std::string& key) const
    {
        static Scene s;
        s = scene();
        for (const Placed& p : s.elements)
            if (p.key == key) return std::get_if<Button>(&p.item);
        return nullptr;
    }
};

// Asks for wake-ups the way the media provider does from its reader's posted
// work: outside any dispatch, nothing on the island changing (decision 163).
struct WakeAsker : Provider {
    Host* host = nullptr;
    int woken = 0;
    const char* id() const override { return "wake.asker"; }
    void start(Host& h) override { host = &h; }
    void stop() override {}
    void onAction(const Action&) override {}
    void onWake() override { ++woken; }
};

void runWakeHookTests()
{
    Island island;
    int armed = 0;
    island.registry.onWakesChanged([&] { ++armed; });
    auto asker = std::make_unique<WakeAsker>();
    WakeAsker* p = asker.get();
    island.registry.add(std::move(asker));
    island.registry.start("wake.asker");

    p->host->wakeAt(Ms{ 1500 });
    expect(armed == 1 && island.registry.nextWake() == Ms{ 1500 },
           "a wake asked outside a dispatch tells the app at once (media gone: ended without a click)");
    p->host->wakeAt(Ms{ 1500 });
    expect(armed == 1, "the same wake again: no re-arm");
    p->host->wakeAt(std::nullopt);
    expect(armed == 2 && !island.registry.nextWake(), "cancelling a wake tells the app too");

    p->host->wakeAt(Ms{ 1500 });
    island.registry.wakeDue(Ms{ 1500 });
    expect(p->woken == 1 && !island.registry.nextWake(), "the wake comes when due");
}

} // namespace

void runProviderTests()
{
    runWakeHookTests();

    Island island;
    island.registry.add(std::make_unique<DemoMediaProvider>());
    island.registry.start("demo");
    expect(island.registry.isRunning("demo"), "demo running");
    expect(mode(island.state) == Mode::Compact, "demo media shows in compact");
    expect(island.state.activities[0].providerId == "demo", "host stamps the provider id");

    Scene compact = island.scene();
    expect(compact.elements.size() == 3, "compact: art + title + waveform");
    const Text* title = compact.elements.size() == 3 ? std::get_if<Text>(&compact.elements[1].item) : nullptr;
    expect(title && title->marquee && title->value.find("Ocean Drive") == 0, "compact: the title swims between the squares");
    const Waveform* bars = compact.elements.size() == 3 ? std::get_if<Waveform>(&compact.elements[2].item) : nullptr;
    expect(bars && bars->active, "playing shows the moving waveform");

    island.state = reduce(island.state, PillClicked{}, island.now);
    expect(mode(island.state) == Mode::Expanded, "click expands");
    const std::string playPause = "expandedBottom/controls/1";
    expect(island.button(playPause) && island.button(playPause)->glyph == Glyph::Pause, "playing shows pause");

    island.now = Ms{ 5000 };
    island.click(playPause);
    expect(island.button(playPause)->glyph == Glyph::Play, "play/pause button pauses");
    expect(mode(island.state) == Mode::Expanded, "button click doesn't collapse");
    const Tokens& tk = tokens();
    Scene card = island.scene();
    for (const Placed& p : card.elements) {
        if (p.key == playPause)
            expect(std::abs(p.frame.y + p.frame.height - (card.size.y - tk.expandedPadding)) < 0.01f,
                   "the tallest control, play, keeps the same bottom margin as the sides");
        if (p.key == "expandedBottom/controls/2") {
            const Placed* play = nullptr;
            for (const Placed& q : card.elements)
                if (q.key == playPause) play = &q;
            expect(play && std::abs((p.frame.y + p.frame.height / 2) - (play->frame.y + play->frame.height / 2)) < 0.01f,
                   "next sits on play's middle");
        }
    }
    // Decision 094: play is the hero, a filled circle; the card grows to fit it under a bigger cover.
    const Button* hero = island.button(playPause);
    expect(hero && hero->primary && !hero->compact, "play is the card's primary action, the full circle (decision 107)");
    const Text& song = std::get<Text>(island.state.activities.front().content.expandedCenter[0].items[0]);
    expect(song.style == TextStyle::CardTitle && song.marquee, "the song's title: big, and swims when too long, like the pill's");
    // Every card and Home share Expanded's height (decision 107): media's rows fit it, or tabs resize the island again.
    expect(card.size.y == tk.expandedSize.y, "the media card is Expanded's height");
    // ...and its parts fill it (decision 107): the cover, its rows and controls need all but a few pixels of it.
    const Slot& under = island.state.activities.front().content.expandedBottom;
    float needed = 2 * tk.expandedPadding + tk.expandedArtSize + tk.slotGap + slotHeight(under, { 0, 0, card.size.x, card.size.y }, {}, {});
    expect(needed <= card.size.y, "media's rows fit the card");
    // ...and the spare goes evenly above each bottom row (decision 119): the bar halfway between the cover and the controls.
    const Placed *cover = nullptr, *bar = nullptr, *controls = nullptr;
    for (const Placed& p : card.elements) {
        if (p.key == "expandedLeading/0/0") cover = &p;
        if (std::holds_alternative<Progress>(p.item)) bar = &p;
        if (p.key == playPause) controls = &p;
    }
    if (cover && bar && controls) {
        float above = bar->frame.y - (cover->frame.y + cover->frame.height), below = controls->frame.y - (bar->frame.y + bar->frame.height);
        expect(std::abs(above - below) < 6 && above > 0, "the bar between the cover and the controls: " + std::to_string(int(above)) + " / " + std::to_string(int(below)));
    } else {
        expect(false, "media's cover, bar and controls placed");
    }
    for (const Placed& p : card.elements)
        if (p.key == "expandedLeading/0/0")
            expect(p.raised && p.frame.width == tk.expandedArtSize, "the cover is raised (a shadow under it), at its full size");
    for (const Placed& p : card.elements)
        if (p.key == "compactLeading/0/0") expect(!p.raised, "only the expanded cover is raised");
    Scene paused = island.scene();
    const Progress* progress = nullptr;
    for (const Placed& p : paused.elements)
        if (p.key == "expandedBottom/bar/1") progress = std::get_if<Progress>(&p.item);
    expect(progress && !progress->timeline.playing && progress->timeline.position == Ms{ 5000 },
           "pause freezes the timeline where it was");

    // Shuffle and repeat are toggles that light up, a pair at the row's right
    // edge, apart from the skips (decisions 168, 180).
    const std::string shuffle = "expandedBottom/controls/3", repeat = "expandedBottom/controls/4";
    {
        Scene controls = island.scene();
        auto at = [&](const std::string& key) {
            for (const Placed& p : controls.elements)
                if (p.key == key) return p.frame;
            return Rect{};
        };
        Rect prev = at("expandedBottom/controls/0"), next = at("expandedBottom/controls/2"), sh = at(shuffle), re = at(repeat), bar = at("expandedBottom/bar/1");
        expect(std::abs(re.x + re.width - (controls.size.x - tk.expandedPadding)) < 0.01f && std::abs(sh.x + sh.width + tk.itemSpacing - re.x) < 0.01f &&
                   next.x + next.width + tk.looseItemSpacing < sh.x,
               "shuffle then repeat at the card's right edge, apart from previous and next");
        Rect play = at("expandedBottom/controls/1");
        expect(std::abs(play.x + play.width / 2 - controls.size.x / 2) < 0.01f && std::abs((play.x - prev.x - prev.width) - (next.x - play.x - play.width)) < 0.01f,
               "the transport still centred on the card, with nothing at the left edge to balance");
        Rect elapsed = at("expandedBottom/bar/0"), left = at("expandedBottom/bar/2");
        expect(elapsed.width < tk.timeLabelWidth && left.width < tk.timeLabelWidth && elapsed.x + elapsed.width + tk.itemSpacing == bar.x,
               "the times as wide as they get, close to the bar: no fixed column");
    }
    expect(island.button(shuffle) && island.button(shuffle)->glyph == Glyph::Shuffle && island.button(shuffle)->on == false,
           "shuffle shows, off");
    expect(island.button(shuffle)->secondary && island.button(repeat)->secondary, "shuffle and repeat are quiet until on (decision 094)");
    island.click(shuffle);
    expect(island.button(shuffle)->on == true, "shuffle click turns it on");
    island.click(shuffle);
    island.click(repeat);
    expect(island.button(repeat)->glyph == Glyph::Repeat && island.button(repeat)->on == true, "repeat: off -> all");
    island.click(repeat);
    expect(island.button(repeat)->glyph == Glyph::RepeatOne && island.button(repeat)->on == true, "repeat: all -> one");
    island.click(repeat);
    expect(island.button(repeat)->on == false, "repeat: one -> off");

    island.click("expandedBottom/controls/2"); // next
    expect(island.state.alertQueue.size() == 1 && island.state.alertQueue[0].coalesceKey == "track",
           "next track queues a track alert (shown once Expanded closes)");
    expect(island.state.alertQueue[0].providerId == "demo", "alert carries the provider id");
    island.state = reduce(island.state, ClickedOutside{}, island.now);
    expect(mode(island.state) == Mode::Alert && island.state.alert->content.label == "Midnight Tea - The Placeholders",
           "track alert shows the new track");

    island.registry.route({ "other", DemoMediaProvider::kActivityId, "next", 0 });
    expect(island.state.activities[0].content.label == "Midnight Tea - The Placeholders", "actions for other providers are ignored");

    // Ctrl+W (decision 034): a provider that doesn't handle it, like a
    // player, ignores it: the activity stays, nothing is asked.
    island.state = reduce(island.state, ClickedOutside{}, island.now);
    island.state = reduce(island.state, PillClicked{}, island.now);
    auto shown = shownActivity(island.state);
    expect(shown && shown->activityId == DemoMediaProvider::kActivityId, "ctrl+w: the open card is the player's");
    island.registry.closeRequested(*shown);
    expect(findActivity(island.state, DemoMediaProvider::kActivityId) && !island.state.card.dialog, "ctrl+w: the player ignores it");

    island.registry.stop("demo");
    expect(island.state.activities.empty() && !island.registry.isRunning("demo"), "stopping ends the activity");
}

} // namespace cup::test
