#include "core/hit_test.h"

#include <algorithm>
#include <string>

namespace cup {

namespace {

std::optional<Hit> hitIn(const std::vector<Placed>& elements, Vec2 point)
{
    for (const Placed& p : elements) {
        if (!p.hitFrame.contains(point)) continue;
        if (auto* button = std::get_if<Button>(&p.item); button && button->enabled)
            return Hit{ { p.providerId, p.activityId, button->actionId, 0 }, p.key };
        if (auto* toggle = std::get_if<Switch>(&p.item); toggle && toggle->enabled)
            return Hit{ { p.providerId, p.activityId, toggle->actionId, 0 }, p.key };
        if (auto* image = std::get_if<Image>(&p.item); image && !image->actionId.empty())
            return Hit{ { p.providerId, p.activityId, image->actionId, 0 }, p.key };
        if (auto* input = std::get_if<TextInput>(&p.item)) // the app focuses it (InputFocused), nothing is sent yet
            return Hit{ { p.providerId, p.activityId, input->actionId, 0 }, p.key };
        if (auto* text = std::get_if<Text>(&p.item); text && !text->editAction.empty()) // the app starts editing it (decision 043)
            return Hit{ { p.providerId, p.activityId, text->editAction, 0 }, p.key };
        if (auto* progress = std::get_if<Progress>(&p.item); progress && progress->seekable && progress->shape == ProgressShape::Bar && p.frame.width > 0)
            return Hit{ { p.providerId, p.activityId, "seek", seekFraction(p, point.x) }, p.key };
    }
    return std::nullopt;
}

} // namespace

std::optional<Hit> hitTest(const Scene& scene, Vec2 point)
{
    // A toast floats over the content (decision 071): its button first, and the rest of it hides what's under it.
    if (scene.toast && scene.toast->frame.contains(point)) return hitIn(scene.toast->elements, point);
    return hitIn(scene.elements, point);
}

std::string rowAt(const Scene& scene, Vec2 point)
{
    auto rowOf = [](const Placed& p) { return p.key.substr(0, p.key.rfind('/')); };
    auto isRow = [](const Placed& p) { return p.key.rfind("tab/", 0) != 0; };
    if (scene.toast && scene.toast->frame.contains(point)) return {}; // a floating toast covers the rows (decision 071)
    for (const Placed& p : scene.elements)
        if (isRow(p) && p.hitFrame.contains(point)) return rowOf(p);
    // Between its items: the band their hit frames span top to bottom (they reach half the gap to the next row).
    if (point.x < 0 || point.x >= scene.size.x) return {};
    for (const Placed& p : scene.elements)
        if (isRow(p) && point.y >= p.hitFrame.y && point.y < p.hitFrame.y + p.hitFrame.height) return rowOf(p);
    return {};
}

float seekFraction(const Placed& progress, float x)
{
    if (progress.frame.width <= 0) return 0;
    return std::clamp((x - progress.frame.x) / progress.frame.width, 0.0f, 1.0f);
}

} // namespace cup
