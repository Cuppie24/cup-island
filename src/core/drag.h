#pragma once
// A drag over the island (decision 061): Receptive and its zones, the grace
// after the drag leaves, and the drop. Part of reduce(); split out of
// island.cpp to keep it readable.

#include "core/island.h"

#include <optional>
#include <string>

namespace cup {

void enterDrag(IslandState& state, const DragEntered& e);    // no zones: nobody takes it, nothing changes
void changeDragZones(IslandState& state, const DragZonesChanged& e);
void hoverZone(IslandState& state, const DropHovered& e);
void leaveDrag(IslandState& state, Ms now);                  // Receptive holds for dropLeaveGrace
// Ends the drag. The card to open: the zone's, if it landed on an enabled one.
std::optional<std::string> dropOnZone(IslandState& state, const Dropped& e, Ms now);

// After every event: a drag that left and didn't come back within the grace ends.
void settleDrag(IslandState& state, Ms now);
std::optional<Ms> dragDeadline(const IslandState& state); // when that grace runs out

// A zone of the drag over the island, by id: nullptr if there's none.
const DropZone* findZone(const IslandState& state, const std::string& id);

} // namespace cup
