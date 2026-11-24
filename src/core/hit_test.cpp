#include "core/hit_test.h"

#include "core/segments.h"

#include <algorithm>
#include <string>

namespace cup {

std::optional<Hit> hitIn(const std::vector<Placed>& elements, Vec2 point)
{
    for (const Placed& p : elements) {
        if (!p.hitFrame.contains(point)) continue;
        if (auto* button = std::get_if<Button>(&p.item); button && button->enabled)
            return Hit{ { p.providerId, p.activityId, button->actionId, 0 }, p.key };
        if (auto* segments = std::get_if<Segments>(&p.item); segments && segments->enabled) // the option under it (decision 093)
            if (auto option = segmentAt(*segments, p.frame, point.x))
                return Hit{ { p.providerId, p.activityId, segmentAction(*segments, *option), 0 }, p.key };
        if (auto* toggle = std::get_if<Switch>(&p.item); toggle && toggle->enabled)
            return Hit{ { p.providerId, p.activityId, toggle->actionId, 0 }, p.key };
        if (auto* image = std::get_if<Image>(&p.item); image && !image->actionId.empty())
            return Hit{ { p.providerId, p.activityId, image->actionId, 0 }, p.key };
        if (auto* input = std::get_if<TextInput>(&p.item)) // the app focuses it (InputFocused), nothing is sent yet
            return Hit{ { p.providerId, p.activityId, input->actionId, 0 }, p.key };
        if (auto* text = std::get_if<Text>(&p.item); text && !text->editAction.empty()) // the app starts editing it (decision 043)
            return Hit{ { p.providerId, p.activityId, text->editAction, 0 }, p.key };
        if (auto* progress = std::get_if<Progress>(&p.item); progress && progress->seekable && progress->shape == ProgressShape::Bar && p.frame.width > 0)
            return Hit{ { p.providerId, p.activityId, seekAction(*progress), seekFraction(p, point.x) }, p.key };
    }
    return std::nullopt;
}

std::optional<Hit> hitTest(const Scene& scene, Vec2 point)
{
    // A row's menu is over everything (decision 086): it hides what's under it; menuItemAt says which item.
    if (scene.menu && scene.menu->frame.contains(point)) return std::nullopt;
    // A toast floats over the content (decision 071): its button first, and the rest of it hides what's under it.
    if (scene.toast && scene.toast->frame.contains(point)) return hitIn(scene.toast->elements, point);
    return hitIn(scene.elements, point);
}

std::string rowAt(const Scene& scene, Vec2 point)
{
    auto rowOf = [](const Placed& p) { return p.key.substr(0, p.key.rfind('/')); };
    auto isRow = [](const Placed& p) { return p.key.rfind("tab/", 0) != 0; };
    if (scene.toast && scene.toast->frame.contains(point)) return {}; // a floating toast covers the rows (decision 071)
    if (scene.menu && scene.menu->frame.contains(point)) return {};   // ...and a row's menu (decision 086)
    for (const Placed& p : scene.elements)
        if (isRow(p) && p.hitFrame.contains(point)) return rowOf(p);
    // Between its items: the band their hit frames span top to bottom (they reach half the gap to the next row).
    if (point.x < 0 || point.x >= scene.size.x) return {};
    for (const Placed& p : scene.elements)
        if (isRow(p) && point.y >= p.hitFrame.y && point.y < p.hitFrame.y + p.hitFrame.height) return rowOf(p);
    return {};
}

bool lightsAt(const Placed& p, Vec2 point)
{
    if (std::holds_alternative<Button>(p.item) && !p.litByRow) return p.frame.contains(point);
    return p.hitFrame.contains(point);
}

std::string seekAction(const Progress& bar) { return bar.actionId.empty() ? "seek" : bar.actionId; }

float seekFraction(const Placed& progress, float x)
{
    if (progress.frame.width <= 0) return 0;
    return std::clamp((x - progress.frame.x) / progress.frame.width, 0.0f, 1.0f);
}

bool inCollapseZone(const Scene& scene, Vec2 point)
{
    return scene.collapseZone && scene.collapseZone->contains(point);
}

Scene scrubbedTimes(Scene scene, const std::string& bar, float fraction)
{
    const Progress* dragged = nullptr;
    for (const Placed& p : scene.elements)
        if (p.key == bar) dragged = std::get_if<Progress>(&p.item);
    if (!dragged || dragged->timeline.duration <= Ms{ 0 }) return scene;
    Timeline timeline = dragged->timeline;
    Ms position{ static_cast<long long>(static_cast<double>(timeline.duration.count()) * std::clamp(fraction, 0.0f, 1.0f)) };
    std::string row = bar.substr(0, bar.rfind('/') + 1); // "<slot>/<row>/"
    for (Placed& p : scene.elements) {
        auto* text = std::get_if<Text>(&p.item);
        if (!text || p.ticking != timeline || p.key.compare(0, row.size(), row) != 0) continue;
        // A laid-out TimeLabel is a Text (slot_layout): a remaining one is right-aligned, a countdown centred.
        std::string left = formatDuration(timeline.duration - position);
        text->value = text->centred ? left : text->alignEnd ? "-" + left : formatDuration(position);
    }
    return scene;
}

} // namespace cup
