#pragma once
// Every number that controls how the island looks and feels. Tune here;
// nothing else hard-codes sizes or timings.

#include "core/geometry.h"

#include <chrono>
#include <cstddef>

namespace cup::tokens {

using Ms = std::chrono::milliseconds;

// Render layer: sized once to the largest pill plus room for the press
// overshoot, and never resized (TR §5, §9.1).
inline constexpr Vec2 kCanvas{ 420, 150 };

// Pill size per mode (PS §3.1). Height 0 = tucked into the screen edge.
inline constexpr Vec2 kIdleSize{ 160, 28 };
inline constexpr Vec2 kCompactSize{ 220, 32 };
inline constexpr Vec2 kAlertSize{ 320, 64 };
inline constexpr Vec2 kExpandedSize{ 380, 108 };
inline constexpr Vec2 kReceptiveSize{ 380, 84 };
inline constexpr Vec2 kHiddenSize{ 160, 0 };

// Morph between modes: cubic-bezier ease-out, 250-350 ms budget (TR §5).
inline constexpr Ms kMorphDuration{ 300 };
inline constexpr Vec2 kMorphEase1{ 0.16f, 1.0f };
inline constexpr Vec2 kMorphEase2{ 0.30f, 1.0f };

// Expanded detail content fades in slightly after the pill starts growing.
inline constexpr Ms kDetailFadeDuration{ 200 };
inline constexpr Ms kDetailFadeDelay{ 100 };

// Press feedback: squash on mouse-down, spring back with overshoot on release.
inline constexpr float kPressScale = 0.96f;
inline constexpr Ms kPressDownDuration{ 100 };
inline constexpr Vec2 kPressDownEase1{ 0.4f, 0.0f };
inline constexpr Vec2 kPressDownEase2{ 0.2f, 1.0f };
inline constexpr Ms kPressReleaseDuration{ 220 };
inline constexpr Vec2 kPressReleaseEase1{ 0.34f, 1.56f }; // y > 1 = overshoot
inline constexpr Vec2 kPressReleaseEase2{ 0.64f, 1.0f };

// Arbitration (PS §6).
inline constexpr Ms kAlertDuration{ 3000 };
inline constexpr Ms kExpandedIdleTimeout{ 8000 };
inline constexpr Ms kHeldAlertMaxAge{ 30000 };
inline constexpr std::size_t kAlertQueueCapacity = 5;

} // namespace cup::tokens
