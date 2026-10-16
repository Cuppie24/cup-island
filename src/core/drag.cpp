#include "core/drag.h"

#include "core/alert_queue.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// The drag is over: whatever was open underneath carries on, its idle time
// counted from now (it didn't run during the drag).
void endDrag(IslandState& s, Ms now)
{
    s.dragging = false;
    s.dropZones.clear();
    s.dropHover.clear();
    s.dragLeftAt.reset();
    s.lastPointerAt = now;
}

} // namespace

void enterDrag(IslandState& s, const DragEntered& e)
{
    if (e.zones.empty()) return;
    bool back = s.dragging; // came back within the grace: the same drag
    s.dragging = true;
    s.dropZones = e.zones;
    s.dragLeftAt.reset();
    if (!back) s.dropHover.clear();
    suspendShowingAlert(s);
}

void changeDragZones(IslandState& s, const DragZonesChanged& e)
{
    if (!s.dragging || e.zones.empty()) return;
    s.dropZones = e.zones;
    const DropZone* hovered = findZone(s, s.dropHover);
    if (!hovered || !hovered->enabled) s.dropHover.clear();
}

void hoverZone(IslandState& s, const DropHovered& e)
{
    if (!s.dragging) return;
    const DropZone* zone = findZone(s, e.zone);
    s.dropHover = zone && zone->enabled ? e.zone : std::string(); // a greyed zone takes nothing, so nothing lights
}

void leaveDrag(IslandState& s, Ms now)
{
    if (!s.dragging || s.dragLeftAt) return;
    s.dragLeftAt = now;
    s.dropHover.clear();
}

std::optional<std::string> dropOnZone(IslandState& s, const Dropped& e, Ms now)
{
    if (!s.dragging) return std::nullopt;
    const DropZone* zone = findZone(s, e.zone);
    std::optional<std::string> card;
    if (zone && zone->enabled) card = zone->cardId;
    endDrag(s, now);
    return card;
}

void settleDrag(IslandState& s, Ms now)
{
    if (auto end = dragDeadline(s); end && now >= *end) endDrag(s, now);
}

std::optional<Ms> dragDeadline(const IslandState& s)
{
    if (!s.dragging || !s.dragLeftAt) return std::nullopt;
    return *s.dragLeftAt + tokens().dropLeaveGrace;
}

const DropZone* findZone(const IslandState& s, const std::string& id)
{
    if (id.empty()) return nullptr;
    auto it = std::find_if(s.dropZones.begin(), s.dropZones.end(), [&](const DropZone& z) { return z.id == id; });
    return it == s.dropZones.end() ? nullptr : &*it;
}

} // namespace cup
