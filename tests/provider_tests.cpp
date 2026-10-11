// Providers end to end, without Windows: registry + demo provider + reduce()
// + layout() + hitTest(), the same loop the app runs.

#include "core/hit_test.h"
#include "core/layout.h"
#include "expect.h"
#include "providers/demo/demo_media_provider.h"
#include "providers/provider_registry.h"

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

} // namespace

void runProviderTests()
{
    Island island;
    island.registry.add(std::make_unique<DemoMediaProvider>());
    island.registry.start("demo");
    expect(island.registry.isRunning("demo"), "demo running");
    expect(mode(island.state) == Mode::Compact, "demo media shows in compact");
    expect(island.state.activities[0].providerId == "demo", "host stamps the provider id");

    Scene compact = island.scene();
    expect(compact.elements.size() == 2, "compact: art + waveform");
    expect(std::holds_alternative<Waveform>(compact.elements[1].item), "playing shows the waveform");

    island.state = reduce(island.state, PillClicked{}, island.now);
    expect(mode(island.state) == Mode::Expanded, "click expands");
    const std::string playPause = "expandedBottom/1/1";
    expect(island.button(playPause) && island.button(playPause)->glyph == Glyph::Pause, "playing shows pause");

    island.now = Ms{ 5000 };
    island.click(playPause);
    expect(island.button(playPause)->glyph == Glyph::Play, "play/pause button pauses");
    expect(mode(island.state) == Mode::Expanded, "button click doesn't collapse");
    Scene paused = island.scene();
    const Progress* progress = nullptr;
    for (const Placed& p : paused.elements)
        if (p.key == "expandedBottom/0/1") progress = std::get_if<Progress>(&p.item);
    expect(progress && !progress->timeline.playing && progress->timeline.position == Ms{ 5000 },
           "pause freezes the timeline where it was");

    island.click("expandedBottom/1/2"); // next
    expect(island.state.alertQueue.size() == 1 && island.state.alertQueue[0].coalesceKey == "track",
           "next track queues a track alert (shown once Expanded closes)");
    expect(island.state.alertQueue[0].providerId == "demo", "alert carries the provider id");
    island.state = reduce(island.state, ClickedOutside{}, island.now);
    expect(mode(island.state) == Mode::Alert && island.state.alert->content.label == "Midnight Tea - The Placeholders",
           "track alert shows the new track");

    island.registry.route({ "other", DemoMediaProvider::kActivityId, "next", 0 });
    expect(island.state.activities[0].content.label == "Midnight Tea - The Placeholders", "actions for other providers are ignored");

    island.registry.stop("demo");
    expect(island.state.activities.empty() && !island.registry.isRunning("demo"), "stopping ends the activity");
}

} // namespace cup::test
