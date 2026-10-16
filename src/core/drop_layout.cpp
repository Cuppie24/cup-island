#include "core/drop_layout.h"

#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {

std::vector<Rect> dropZoneFrames(Rect area, std::size_t count)
{
    std::vector<Rect> frames;
    if (count == 0) return frames;
    float gap = tokens().dropZoneGap;
    float width = std::max(0.0f, (area.width - gap * static_cast<float>(count - 1)) / static_cast<float>(count));
    for (std::size_t i = 0; i < count; ++i)
        frames.push_back({ area.x + static_cast<float>(i) * (width + gap), area.y, width, area.height });
    return frames;
}

void placeDropZones(const IslandState& s, Vec2 contentSize, Scene& scene, const MeasureText& measure)
{
    const Tokens& t = tokens();
    float p = t.expandedPadding;
    Rect area{ p, p, std::max(0.0f, contentSize.x - 2 * p), std::max(0.0f, contentSize.y - 2 * p) };
    std::vector<Rect> frames = dropZoneFrames(area, s.dropZones.size());
    scene.tileRadius = std::min(std::max(0.0f, pillCornerRadius(scene.size) - p), area.height / 2);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const DropZone& zone = s.dropZones[i];
        scene.zones.push_back({ frames[i], zone.id, zone.enabled, zone.enabled && zone.id == s.dropHover });
        // A greyed zone's label is quieter; its reason says why (decision 061).
        Slot slot{ row({ Icon{ zone.glyph } }, Align::Center),
                   row({ Text{ .value = zone.label, .style = zone.enabled ? TextStyle::Title : TextStyle::Body, .centred = true } }, Align::Center) };
        if (!zone.reason.empty())
            slot.push_back(row({ Text{ .value = zone.reason, .style = TextStyle::Caption, .centred = true } }, Align::Center));
        Content content{ .label = zone.label };
        Source source{ &content, zone.providerId, zone.cardId };
        Rect inner{ frames[i].x + t.tilePadding, frames[i].y + t.tilePadding,
                    std::max(0.0f, frames[i].width - 2 * t.tilePadding), std::max(0.0f, frames[i].height - 2 * t.tilePadding) };
        placeSlot({ "zone" + std::to_string(i), &slot, inner }, source, scene.tileRadius, scene.now, scene.elements, measure);
    }
    if (!s.dropZones.empty()) scene.label = s.dropZones.front().label;
}

} // namespace cup
