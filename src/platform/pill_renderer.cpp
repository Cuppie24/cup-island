#include "platform/pill_renderer.h"
#include "platform/log.h"

#include "core/tokens.h"
#include "core/tokens_json.h"
#include "platform/element_painting.h"
#include "platform/field_painting.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace cup::platform {

void PillRenderer::build(wuc::Compositor const& c, wuc::ContainerVisual const& root, const Scene& initial)
{
    queue_ = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    device_.create(c);
    applied_ = tokens();
    pill_.build(c, device_, root, initial.size, initial.offset);
    bubble_.build(c, device_, root, {}, initial.bubbleTuck); // above the pill: it's dragged over it
    bridge_.build(c, root, pill_, bubble_);
    apply(initial);
}

void PillRenderer::refreshAnimations()
{
    bool coloursOnly = onlyColorsDiffer(applied_, tokens());
    applied_ = tokens();
    pill_.refreshAnimations(coloursOnly);
    bubble_.refreshAnimations(coloursOnly);
    bridge_.refreshAnimations();
    measured_.clear();
}

float PillRenderer::measure(const Text& text)
{
    if (!device_.dwrite()) return 0; // before build(): nothing laid out needs it yet
    auto key = std::make_pair(text.value, text.style);
    auto it = measured_.find(key);
    if (it != measured_.end()) return it->second;
    // Titles change with every track: don't grow forever. Roomy, as wrapping
    // to-dos measure each of their words (decision 045).
    if (measured_.size() > 2048) measured_.clear();
    return measured_[key] = measureText(device_.dwrite(), text);
}

std::size_t PillRenderer::caretAt(const Placed& element, Vec2 point)
{
    if (!device_.dwrite()) return 0;
    Vec2 size{ element.frame.width, element.frame.height };
    Vec2 local{ point.x - element.frame.x, point.y - element.frame.y }; // drawn from the frame's corner, at its size (element_visuals paint)
    if (auto* input = std::get_if<TextInput>(&element.item)) return platform::caretAt(device_.dwrite(), *input, size, local);
    if (auto* text = std::get_if<Text>(&element.item)) return platform::caretAt(device_.dwrite(), *text, size, local);
    return 0;
}

std::optional<std::size_t> PillRenderer::caretOnLine(const Placed& field, int direction)
{
    auto* input = std::get_if<TextInput>(&field.item);
    if (!device_.dwrite() || !input) return std::nullopt;
    return platform::caretOnLine(device_.dwrite(), *input, { field.frame.width, field.frame.height }, direction);
}

void PillRenderer::apply(const Scene& scene)
{
    // Geometry first, content later (decision 012). Building content means new
    // surfaces and Direct2D drawing for every element that appears, and while
    // this handler runs, Composition can't commit the new Size: the morph
    // would start only after all of it, which reads as lag on mouse-up.
    // Lifted (decision 026): it follows the pointer sideways at once, no
    // morph; let go, it glides to where it lands, snapping. Placed first, so
    // the glide knows where that is.
    pill_.place(scene.size, scene.offset);
    pill_.lift(scene.lifted ? std::optional<float>(scene.offset.x + scene.size.x / 2) : std::nullopt);
    // A side flush with the screen's edge has no ear: it would hang off the
    // monitor. In Split the right edge is the bubble's.
    pill_.setEars(!scene.flushLeft, !(scene.flushRight && !scene.bubble));
    pill_.setHighlighted(scene.dropTarget);
    const TabStrip* tabs = scene.tabs ? &*scene.tabs : nullptr;
    std::vector<Rect> tabFrames;
    if (tabs)
        for (const auto& tab : tabs->tabs) tabFrames.push_back(tab.frame);
    if (tabs && tabs->pin) tabFrames.push_back(tabs->pin->frame); // its faint background, like Home's button
    pill_.setTabs(tabFrames, tabs ? tabs->radius : 0);
    pill_.setPinnedTab(tabs && tabs->pin && tabs->pinned ? std::optional<Rect>(tabs->pin->frame) : std::nullopt, tabs ? tabs->radius : 0);
    pill_.setSelectedTab(tabs && tabs->active ? std::optional<Rect>(tabs->tabs[*tabs->active].frame) : std::nullopt, tabs ? tabs->radius : 0);
    // Nothing left that lights up as an area: drop a stale hover. A list row's
    // stays: the app re-aims it after every scene, and clearing it here would
    // blink it on each tick of a countdown on the same card.
    bool listRows = std::any_of(scene.elements.begin(), scene.elements.end(), [](const Placed& p) { return p.listRow.has_value(); });
    if (!tabs && scene.tiles.empty() && scene.zones.empty() && !listRows) pill_.setHovered(std::nullopt, 0);
    std::vector<Rect> tiles;
    for (const Tile& tile : scene.tiles) tiles.push_back(tile.frame);
    for (const ZoneMark& zone : scene.zones) tiles.push_back(zone.frame); // a drag's drop zones look like Home's widgets (decision 061)
    pill_.setTiles(tiles, scene.tileRadius);
    pill_.setBadges(scene.badges);

    // While the bubble is dragged it just follows the cursor: no neck, no
    // other motion (decision 007). The neck is for budding and settling.
    const BubbleScene* bubble = scene.bubble ? &*scene.bubble : nullptr;
    bridge_.setEnabled(!(bubble && bubble->dragCentre));
    if (bubble) {
        bubble_.place({ bubble->frame.width, bubble->frame.height }, { bubble->frame.x, bubble->frame.y });
        bubble_.setEars(true, !scene.flushRight);
        bubble_.follow(bubble->dragCentre);
    } else {
        // No bubble: shrink into the pill (it buds back out of the same spot).
        bubble_.follow(std::nullopt);
        bubble_.place({}, scene.bubbleTuck);
    }

    // Low priority runs after the compositor's own commit work. Only the
    // latest scene matters, so a burst of applies (pointer moves) queues one call.
    bool queued = pendingContent_.has_value();
    pendingContent_ = scene;
    auto run = [this] {
        // Outside the window procedure's try: an escaping exception would end the process.
        try {
            applyContent();
        } catch (winrt::hresult_error const& e) {
            logLine("WinRT error building content: 0x%08x %ls", static_cast<unsigned int>(e.code()), e.message().c_str());
        }
    };
    if (!queued && !queue_.TryEnqueue(winrt::Windows::System::DispatcherQueuePriority::Low, run))
        run(); // queue shutting down: do it now
}

void PillRenderer::applyContent()
{
    if (!pendingContent_) return;
    Scene scene = *std::exchange(pendingContent_, std::nullopt);
    auto start = std::chrono::steady_clock::now();
    pill_.setContent(scene.elements, scene.now);
    bubble_.setContent(scene.bubble ? scene.bubble->elements : std::vector<Placed>{}, scene.now);

    // Debug builds have a console: flag slow content builds when chasing lag.
    auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    if (took.count() > 8) {
        logLine("content took %lld ms", static_cast<long long>(took.count()));
    }
}

} // namespace cup::platform
