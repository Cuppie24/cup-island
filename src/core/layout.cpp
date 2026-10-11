#include "core/layout.h"

#include "core/tokens.h"

namespace cup {
namespace {

Vec2 sizeFor(Mode mode)
{
    const Tokens& t = tokens();
    switch (mode) {
    case Mode::Idle: return t.idleSize;
    case Mode::Compact: return t.compactSize;
    case Mode::Split: return t.compactSize;
    case Mode::Alert: return t.alertSize;
    case Mode::Expanded: return t.expandedSize;
    case Mode::Receptive: return t.receptiveSize;
    case Mode::Hidden: return t.hiddenSize;
    }
    return t.idleSize;
}

Content contentFor(const IslandState& s, Mode mode)
{
    if (mode == Mode::Alert) return s.alert->content;
    // Expanded shows the activity that was opened; resting shows the top one.
    for (const auto& a : s.activities)
        if (mode != Mode::Expanded || a.id == s.expandedId) return a.content;
    return {};
}

} // namespace

Scene layout(const IslandState& state)
{
    Scene scene;
    scene.mode = mode(state);
    scene.size = sizeFor(scene.mode);
    scene.offset = { (tokens().canvas.x - scene.size.x) / 2, 0 };
    scene.detailOpacity = scene.mode == Mode::Expanded ? 1.0f : 0.0f;
    scene.showBubble = scene.mode == Mode::Split;
    scene.content = contentFor(state, scene.mode);
    return scene;
}

} // namespace cup
