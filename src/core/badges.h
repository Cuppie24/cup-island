#pragma once
// Notification badges (decision 038): a dot on whatever has something new,
// so the way to it is marked. Providers set the flags: Content::badge (its
// widget on Home, its tab) and Button::badge (one button on a card). Where
// each dot goes is decided here, once, for every kind of element; the
// renderer draws them all alike.

#include "core/layout.h"

#include <vector>

namespace cup {

// The centres (pill coordinates) of the scene's dots: a widget's or tab's in
// its top-right corner, concentric with its rounding (badgeInset in at
// least); a button's on its round outline, top right. A button scrolled out
// of its list's window has none.
std::vector<Vec2> badgeCentres(const Scene& scene);

} // namespace cup
