#include "core/layout.h"

#include "core/tokens.h"

namespace cup {
namespace {

Vec2 sizeFor(Mode mode)
{
    switch (mode) {
    case Mode::Idle: return tokens::kIdleSize;
    case Mode::Compact: return tokens::kCompactSize;
    case Mode::Split: return tokens::kCompactSize;
    case Mode::Alert: return tokens::kAlertSize;
    case Mode::Expanded: return tokens::kExpandedSize;
    case Mode::Receptive: return tokens::kReceptiveSize;
    case Mode::Hidden: return tokens::kHiddenSize;
    }
    return tokens::kIdleSize;
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
    scene.offset = { (tokens::kCanvas.x - scene.size.x) / 2, 0 };
    scene.detailOpacity = scene.mode == Mode::Expanded ? 1.0f : 0.0f;
    scene.showBubble = scene.mode == Mode::Split;
    scene.content = contentFor(state, scene.mode);
    return scene;
}

} // namespace cup
