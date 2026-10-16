#pragma once
// Receptive's drop zones (decision 061): side by side across the card, each
// as tall as it is, with its icon, its label and the reason under it. Part
// of layout(); split out of layout.cpp to keep it readable.

#include "core/layout.h"

#include <vector>

namespace cup {

// `count` zones side by side in `area`, dropZoneGap apart.
std::vector<Rect> dropZoneFrames(Rect area, std::size_t count);

// Adds the drag's zones to the scene: their marks and their content.
void placeDropZones(const IslandState& state, Vec2 contentSize, Scene& scene, const MeasureText& measure);

} // namespace cup
