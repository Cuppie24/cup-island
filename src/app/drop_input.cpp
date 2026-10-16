#include "app/drop_input.h"

#include "core/drag.h"
#include "providers/provider_registry.h"

#include <memory>

namespace cup::app {
namespace {

// The zone under a screen point, if it's an enabled one of this drag.
const DropZone* zoneUnder(const DropHooks& h, POINT screen)
{
    const ZoneMark* mark = zoneAt(h.scene(), h.toPill(screen));
    return mark ? findZone(h.state(), mark->id) : nullptr;
}

} // namespace

platform::DropCallbacks dropCallbacks(DropHooks hooks)
{
    auto h = std::make_shared<DropHooks>(std::move(hooks));
    platform::DropCallbacks c;
    c.onEnter = [h](const DragSummary& drag) {
        ProviderRegistry* providers = h->providers();
        h->dispatch(DragEntered{ providers ? providers->dropZones(drag) : std::vector<DropZone>{} }, "drag");
    };
    c.onSizes = [h](const DragSummary& drag) {
        if (ProviderRegistry* providers = h->providers()) h->dispatch(DragZonesChanged{ providers->dropZones(drag) }, "drag sizes");
    };
    c.onOver = [h](POINT screen) {
        if (!h->state().dragging) return DropEffect::None; // nobody took it (or Hidden)
        const DropZone* zone = zoneUnder(*h, screen);
        std::string id = zone ? zone->id : std::string();
        if (id != h->state().dropHover) h->dispatch(DropHovered{ id }, "drag over");
        return zone ? zone->effect : DropEffect::None;
    };
    c.onLeave = [h] { h->dispatch(DragLeft{}, "drag left"); };
    c.onDrop = [h](POINT screen, const std::vector<DroppedItem>& items) {
        if (!h->state().dragging) return DropEffect::None;
        const DropZone* found = zoneUnder(*h, screen);
        std::optional<DropZone> zone = found ? std::optional<DropZone>(*found) : std::nullopt; // the state changes below
        h->dispatch(Dropped{ zone ? zone->id : std::string() }, "drop");
        if (!zone) return DropEffect::None;
        if (ProviderRegistry* providers = h->providers()) providers->drop(*zone, items);
        // The card opened like any other: keys reach it, and a click elsewhere
        // closes it. Windows may refuse; then outside_clicks covers the click (decision 063).
        if (h->window) SetForegroundWindow(h->window);
        return zone->effect;
    };
    return c;
}

void toggleDemoDrag(const DropHooks& h)
{
    if (h.state().dragging) {
        h.dispatch(DragLeft{}, "demo key");
        return;
    }
    DragSummary drag{ .items = { { DragKind::File, "Report.pdf", 2u * 1024 * 1024 } }, .total = 1, .sizesKnown = true };
    ProviderRegistry* providers = h.providers();
    h.dispatch(DragEntered{ providers ? providers->dropZones(drag) : std::vector<DropZone>{} }, "demo key");
}

} // namespace cup::app
