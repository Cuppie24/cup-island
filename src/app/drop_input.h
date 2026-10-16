#pragma once
// The app's side of a drag over the island (decision 061): asks providers
// for their zones (the registry), tells core (DragEntered, DropHovered,
// Dropped), and hands a drop to its zone's provider. Where a point lands is
// core's (zoneAt); this keeps nothing but the zone last hovered.

#include "core/island.h"
#include "core/layout.h"
#include "platform/drop_target.h"

#include <functional>

namespace cup {
class ProviderRegistry;
}

namespace cup::app {

struct DropHooks {
    std::function<const IslandState&()> state;
    std::function<const Scene&()> scene;
    std::function<void(const Event&, const char* reason)> dispatch;
    std::function<ProviderRegistry*()> providers;
    std::function<Vec2(POINT screen)> toPill; // a screen point in the pill's coordinates
};

// The drop target's callbacks, wired to the island. Set postToUi, storeFolder
// and maxChecked on the result before registering it.
platform::DropCallbacks dropCallbacks(DropHooks hooks);

// The D demo key: a drag of one small file, as if from Explorer, or its end.
void toggleDemoDrag(const DropHooks& hooks);

} // namespace cup::app
