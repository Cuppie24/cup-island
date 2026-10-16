// A drag's drop zones (decision 061): laid out side by side on Receptive's
// card, found under the pointer only when enabled, and gathered from
// providers and routed back by the registry.

#include "core/drop_layout.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/provider_registry.h"

namespace cup::test {
namespace {

struct Taker : Provider {
    std::vector<std::pair<std::string, std::size_t>> drops; // zone, items
    const char* id() const override { return "taker"; }
    void start(Host&) override {}
    void stop() override {}
    void onAction(const Action&) override {}
    std::vector<DropZone> dropZones(const DragSummary& d) override
    {
        if (d.items.empty()) return {};
        return { DropZone{ .id = "taker.here", .cardId = "taker", .label = "Here" } };
    }
    void onDrop(const std::string& zone, const std::vector<DroppedItem>& items) override { drops.push_back({ zone, items.size() }); }
};

} // namespace

void runDropTests()
{
    const Tokens& t = tokens();
    auto frames = dropZoneFrames({ 10, 10, 408, 100 }, 2);
    expect(frames.size() == 2 && frames[0].width == frames[1].width && frames[0].x == 10 &&
           frames[1].x + frames[1].width == 418 && frames[1].x - (frames[0].x + frames[0].width) == t.dropZoneGap,
           "drop: zones side by side, filling the card, a gap between");

    IslandState s;
    s = reduce(s, DragEntered{ { DropZone{ .providerId = "shelf", .id = "link", .cardId = "shelf", .label = "Link" },
                                 DropZone{ .providerId = "shelf", .id = "copy", .cardId = "shelf", .label = "Copy", .reason = "Too large", .enabled = false } } },
               Ms{ 0 });
    s = reduce(s, DropHovered{ "link" }, Ms{ 0 });
    Scene scene = layout(s, Ms{ 0 });
    expect(scene.mode == Mode::Receptive && scene.size == t.receptiveSize && scene.zones.size() == 2, "drop: Receptive shows its zones");
    expect(scene.zones[0].hovered && !scene.zones[1].hovered && !scene.zones[1].enabled, "drop: the hovered one lights; the greyed one is marked");
    const ZoneMark& link = scene.zones[0];
    const ZoneMark& copy = scene.zones[1];
    Vec2 inLink{ link.frame.x + link.frame.width / 2, link.frame.y + link.frame.height / 2 };
    Vec2 inCopy{ copy.frame.x + copy.frame.width / 2, copy.frame.y + copy.frame.height / 2 };
    Vec2 inGap{ link.frame.x + link.frame.width + t.dropZoneGap / 2, inLink.y };
    expect(zoneAt(scene, inLink) == &scene.zones[0], "drop: a point on a zone finds it");
    expect(!zoneAt(scene, inCopy) && !zoneAt(scene, inGap), "drop: a greyed zone or the gap takes nothing");
    bool reasonShown = false;
    for (const Placed& p : scene.elements) {
        auto* text = std::get_if<Text>(&p.item);
        if (text && text->value == "Too large") reasonShown = copy.frame.contains({ p.frame.x + 1, p.frame.y + 1 });
        expect(p.frame.x >= 0 && p.frame.x + p.frame.width <= scene.size.x, "drop: zone content inside the card: " + p.key);
    }
    expect(reasonShown, "drop: a zone's reason shows on it");

    IslandState state;
    ProviderRegistry registry{ [&](const Event& e, const char*) { state = reduce(state, e, Ms{ 0 }); }, [] { return Ms{ 0 }; } };
    auto taker = std::make_unique<Taker>();
    Taker* seen = taker.get();
    registry.add(std::move(taker));
    DragSummary one{ .items = { { DragKind::File, "a" } }, .total = 1, .sizesKnown = true };
    expect(registry.dropZones(one).empty(), "drop: a stopped provider offers nothing");
    registry.start("taker");
    auto zones = registry.dropZones(one);
    expect(zones.size() == 1 && zones[0].providerId == "taker", "drop: zones are stamped with their provider");
    registry.drop(zones[0], { DroppedItem{ .path = "C:\\a" } });
    DropZone greyed = zones[0];
    greyed.enabled = false;
    registry.drop(greyed, { DroppedItem{} });
    expect(seen->drops.size() == 1 && seen->drops[0].first == "taker.here" && seen->drops[0].second == 1,
           "drop: a drop goes to its zone's provider, never through a greyed zone");
}

} // namespace cup::test
