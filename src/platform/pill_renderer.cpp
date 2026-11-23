#include "platform/pill_renderer.h"
#include "platform/log.h"

#include "core/liquid_glass.h"
#include "core/tokens.h"
#include "core/tokens_json.h"
#include "platform/blob_shape.h"
#include "platform/element_painting.h"
#include "platform/element_parts.h"
#include "platform/field_painting.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace cup::platform {

void PillRenderer::build(wuc::Compositor const& c, wuc::ContainerVisual const& root, const Scene& initial)
{
    queue_ = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    compositor_ = c;
    device_.create(c);
    applied_ = tokens();
    pill_.build(c, device_, root, initial.size, initial.offset);
    bubble_.build(c, device_, root, {}, initial.bubbleTuck); // above the pill: it's dragged over it
    bridge_.build(c, root, pill_, bubble_);
    menu_.build(c, device_, root); // last: over the pill and the bubble
    apply(initial, {});
    liquid_.onLost = [this] {
        liquidFailed_ = true; // the frosted glass, until liquid is picked again
        pill_.setLiquid(nullptr);
    };
    // A fresh picture is up: the open card fades to it (if it's still open).
    liquid_.onReady = [this] { pill_.showLiquid(liquidDrawn(tokens(), cardOpen_)); };
    syncLiquid();
}

void PillRenderer::syncLiquid()
{
    const Tokens& t = tokens();
    bool wanted = compositor_ && window_ && t.glass && t.glassStyle == GlassStyle::Liquid;
    if (!wanted) {
        liquidFailed_ = false;
        if (!liquid_.running()) return;
        pill_.setLiquid(nullptr);
        liquid_.stop();
        return;
    }
    if (!liquid_.running()) {
        if (liquidFailed_) return;
        if (!liquid_.start(compositor_, window_)) {
            liquidFailed_ = true;
            return;
        }
    }
    // The pill only: the bubble is never an open card (decision 172).
    pill_.setLiquid(liquid_.picture());
    liquid_.refresh(); // the tokens it draws with may be new
    activateLiquid();
}

void PillRenderer::layerChanged()
{
    if (!liquid_.running()) return;
    pill_.setLiquid(nullptr);
    liquid_.stop();
    syncLiquid();
}

void PillRenderer::activateLiquid()
{
    if (!liquid_.running()) return;
    bool drawn = liquidDrawn(tokens(), cardOpen_);
    // Collapsing: the frosted glass back at once, as the capture stops. Opening: onReady shows it.
    if (!drawn) pill_.showLiquid(false);
    liquid_.setActive(drawn);
}

void PillRenderer::placeLiquid(const Scene& scene, const Motion& morph)
{
    const Motion atOnce{ Ms{ 0 }, 0, 0 }; // dragged: it's where the pointer is
    Vec2 size = scene.size;
    // Each shape's flared corners as apply() gives them to its Blob (setEars).
    bool pillLeft = !scene.flushLeft, pillRight = !(scene.flushRight && !scene.bubble);
    if (scene.lifted) liquid_.place(0, { scene.offset.x, tokens().liftDrop, size.x, size.y }, false, false, false, atOnce);
    else liquid_.place(0, { scene.offset.x, scene.offset.y, size.x, size.y }, true, pillLeft, pillRight, towardSize(morph, size));
    const BubbleScene* bubble = scene.bubble ? &*scene.bubble : nullptr;
    if (bubble && bubble->dragCentre) {
        const Rect& f = bubble->frame;
        liquid_.place(1, { bubble->dragCentre->x - f.width / 2, bubble->dragCentre->y - f.height / 2, f.width, f.height }, false, false, false, atOnce);
    } else if (bubble) {
        liquid_.place(1, bubble->frame, true, true, !scene.flushRight, morph);
    } else {
        liquid_.place(1, { scene.bubbleTuck.x, scene.bubbleTuck.y, 0, 0 }, true, false, false, towardSize(morph, {}));
    }
}

void PillRenderer::refreshAnimations()
{
    bool coloursOnly = onlyColorsDiffer(applied_, tokens());
    applied_ = tokens();
    pill_.refreshAnimations(coloursOnly);
    bubble_.refreshAnimations(coloursOnly);
    bridge_.refreshAnimations();
    menu_.refresh(coloursOnly);
    measured_.clear();
    syncLiquid(); // glass on or off, or another style (decision 165)
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

void PillRenderer::apply(const Scene& scene, const SceneMotion& motion)
{
    // Geometry first, content later (decision 012). Building content means new
    // surfaces and Direct2D drawing for every element that appears, and while
    // this handler runs, Composition can't commit the new Size: the morph
    // would start only after all of it, which reads as lag on mouse-up.
    // Lifted (decision 026): it follows the pointer sideways at once, no
    // morph; let go, it glides to where it lands, snapping. Placed first, so
    // the glide knows where that is.
    pill_.place(scene.size, scene.offset, motion.morph);
    pill_.lift(scene.lifted ? std::optional<float>(scene.offset.x + scene.size.x / 2) : std::nullopt, motion.fling);
    placeLiquid(scene, motion.morph);
    cardOpen_ = scene.mode == Mode::Expanded;
    activateLiquid();
    if (motion.pulse) pill_.pulse();
    if (motion.reveal > Ms{ 0 }) revealAt_ = std::chrono::steady_clock::now() + motion.reveal;
    // A side flush with the screen's edge has no ear: it would hang off the
    // monitor. In Split the right edge is the bubble's.
    pill_.setEars(!scene.flushLeft, !(scene.flushRight && !scene.bubble));
    pill_.setHighlighted(scene.dropTarget);
    pill_.setCard(scene.mode == Mode::Expanded); // the open card's glow (decision 166); the bubble never is one
    const TabStrip* tabs = scene.tabs ? &*scene.tabs : nullptr;
    // The page row (decision 165): the shown page's icon lit, the shape gliding to a newly picked one.
    std::optional<Rect> lit = tabs && tabs->active ? std::optional<Rect>(tabs->tabs[*tabs->active].lit) : std::nullopt;
    pill_.setSelectedTab(lit, lit ? tabs->tabs[*tabs->active].radius : 0);
    // Nothing left that lights up as an area: drop a stale hover. A list row's
    // stays: the app re-aims it after every scene, and clearing it here would
    // blink it on each tick of a countdown on the same card.
    bool listRows = std::any_of(scene.elements.begin(), scene.elements.end(), [](const Placed& p) { return p.listRow.has_value(); });
    if (!tabs && !listRows) pill_.setHovered(std::nullopt, 0);
    pill_.setBadges(scene.badges, scene.accentBadges);
    // A row's menu at once, not with the queued content: it answers a click or a key (decision 086).
    menu_.show(scene.menu, scene.offset, scene.now);

    // While the bubble is dragged it just follows the cursor: no neck, no
    // other motion (decision 007). The neck is for budding and settling.
    const BubbleScene* bubble = scene.bubble ? &*scene.bubble : nullptr;
    bridge_.setEnabled(!(bubble && bubble->dragCentre));
    if (bubble) {
        bubble_.place({ bubble->frame.width, bubble->frame.height }, { bubble->frame.x, bubble->frame.y }, motion.morph);
        bubble_.setEars(true, !scene.flushRight);
        bubble_.follow(bubble->dragCentre, motion.fling);
    } else {
        // No bubble: shrink into the pill (it buds back out of the same spot).
        bubble_.follow(std::nullopt);
        bubble_.place({}, scene.bubbleTuck, motion.morph);
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
        } catch (std::exception const& e) {
            logLine("Exception building content: %s", e.what()); // bad_alloc and co.: also fatal if let out
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
    Ms reveal = std::max(Ms{ 0 }, std::chrono::duration_cast<Ms>(revealAt_ - start)); // what's left of it after the queue
    pill_.setContent(scene.elements, scene.now, reveal);
    pill_.setToast(scene.toast, scene.now); // floating over it (decision 071)
    bubble_.setContent(scene.bubble ? scene.bubble->elements : std::vector<Placed>{}, scene.now, reveal);

    // Debug builds have a console: flag slow content builds when chasing lag.
    auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    if (took.count() > 8) {
        logLine("content took %lld ms", static_cast<long long>(took.count()));
    }
}

} // namespace cup::platform
