#include "platform/rolling_text.h"

#include "core/slot_layout.h"
#include "core/tokens.h"
#include "platform/element_painting.h"

#include <algorithm>
#include <cmath>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Composition rejects zero-length animations.
Ms atLeast1(Ms d) { return std::max(d, Ms{ 1 }); }

bool paintCell(GraphicsDevice& device, const RollingText& rolling, const RollingCell& cell, float height)
{
    return device.draw(cell.surface, [&](ID2D1DeviceContext* dc) {
        auto draw = [&](ID2D1DeviceContext* target) { paintCharacter(target, device.dwrite(), rolling.text, cell.value, rolling.lead, height); };
        if (rolling.grey) paintGrey(dc, draw); // a muted tab's (decision 072)
        else draw(dc);
    }, /* bigText */ true);
}

// A character's sprite at its place: its surface reaches `lead` past its
// advance on each side, and the container starts `lead` left of the frame,
// so the sprite's left is its advance's.
RollingCell makeCell(Compositor const& c, GraphicsDevice& device, const TextCharacter& character, float lead, float height)
{
    RollingCell cell{ .value = character.value, .left = character.left };
    Vec2 size{ std::ceil(character.width + 2 * lead), std::ceil(height) };
    cell.surface = device.createSurface(size);
    cell.sprite = c.CreateSpriteVisual();
    cell.sprite.Brush(c.CreateSurfaceBrush(cell.surface));
    cell.sprite.Size(toFloat2(size));
    cell.sprite.Offset({ std::round(character.left), 0.0f, 0.0f });
    return cell;
}

// Slides it from `from` to `to` down its line, fading between the two opacities.
void slide(Compositor const& c, Visual const& visual, float from, float to, float fadeFrom, float fadeTo, Bezier curve)
{
    Ms duration = atLeast1(tokens().digitRollDuration);
    auto ease = c.CreateCubicBezierEasingFunction(toFloat2(curve.c1), toFloat2(curve.c2));
    auto offset = c.CreateScalarKeyFrameAnimation();
    offset.InsertKeyFrame(0.0f, from);
    offset.InsertKeyFrame(1.0f, to, ease);
    offset.Duration(duration);
    visual.StartAnimation(L"Offset.Y", offset);
    auto opacity = c.CreateScalarKeyFrameAnimation();
    opacity.InsertKeyFrame(0.0f, fadeFrom);
    opacity.InsertKeyFrame(1.0f, fadeTo, c.CreateCubicBezierEasingFunction(toFloat2(tokens().morphEase.c1), toFloat2(tokens().morphEase.c2)));
    opacity.Duration(duration);
    visual.StartAnimation(L"Opacity", opacity);
}

// The old character drops out below its line and is removed once gone.
void rollOut(Compositor const& c, const RollingCell& cell, float height)
{
    auto batch = c.CreateScopedBatch(CompositionBatchTypes::Animation);
    slide(c, cell.sprite, 0.0f, height, 1.0f, 0.0f, tokens().morphEase);
    batch.End();
    auto sprite = cell.sprite;
    batch.Completed([sprite](auto&&, auto&&) {
        if (auto parent = sprite.Parent()) parent.Children().Remove(sprite);
    });
}

void removeNow(const RollingCell& cell)
{
    if (auto parent = cell.sprite.Parent()) parent.Children().Remove(cell.sprite);
}

} // namespace

float rollingLead(const Text& text) { return std::ceil(fontSize(text.style) * 0.12f); }

RollingText createRollingText(Compositor const& c, GraphicsDevice& device, const Text& text, Vec2 size, bool grey)
{
    RollingText rolling{ .text = text, .lead = rollingLead(text), .grey = grey };
    rolling.visual = c.CreateContainerVisual();
    rolling.visual.Size({ size.x + 2 * rolling.lead, size.y });
    rolling.visual.Clip(c.CreateInsetClip()); // what rolls past the line's top or bottom isn't seen
    for (const TextCharacter& character : charactersOf(device.dwrite(), text, size)) {
        rolling.cells.push_back(makeCell(c, device, character, rolling.lead, size.y));
        rolling.visual.Children().InsertAtTop(rolling.cells.back().sprite);
    }
    repaintRolling(device, rolling, size);
    return rolling;
}

bool rollTo(Compositor const& c, GraphicsDevice& device, RollingText& rolling, const Text& text, Vec2 size)
{
    bool sameValue = text.value == rolling.text.value;
    rolling.text = text;
    if (sameValue) return repaintRolling(device, rolling, size); // its look changed, not its time
    const Tokens& t = tokens();
    std::vector<TextCharacter> characters = charactersOf(device.dwrite(), text, size);
    // Another length ("10:00" to "9:59"): every character moves, so every one rolls.
    bool sameShape = characters.size() == rolling.cells.size();
    std::vector<RollingCell> next;
    bool drawn = true;
    for (std::size_t i = 0; i < characters.size(); ++i) {
        const TextCharacter& character = characters[i];
        if (sameShape && rolling.cells[i].value == character.value && rolling.cells[i].left == character.left) {
            next.push_back(rolling.cells[i]); // unchanged: stays put
            continue;
        }
        RollingCell fresh = makeCell(c, device, character, rolling.lead, size.y);
        drawn = paintCell(device, rolling, fresh, size.y) && drawn;
        rolling.visual.Children().InsertAtTop(fresh.sprite);
        if (sameShape) { // the old one at this place goes as the new one comes
            if (t.reducedMotion) removeNow(rolling.cells[i]);
            else rollOut(c, rolling.cells[i], size.y);
        }
        // Falls in from above, settling with a little bounce (decision 107).
        if (!t.reducedMotion) slide(c, fresh.sprite, -size.y, 0.0f, 0.0f, 1.0f, t.settleEase);
        next.push_back(fresh);
    }
    if (!sameShape)
        for (const RollingCell& cell : rolling.cells) {
            if (t.reducedMotion) removeNow(cell);
            else rollOut(c, cell, size.y);
        }
    rolling.cells = std::move(next);
    return drawn;
}

bool repaintRolling(GraphicsDevice& device, RollingText& rolling, Vec2 size)
{
    bool drawn = true;
    for (const RollingCell& cell : rolling.cells) drawn = paintCell(device, rolling, cell, size.y) && drawn;
    return drawn;
}

} // namespace cup::platform
