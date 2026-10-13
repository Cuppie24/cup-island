# 038: Notification badges

Status: Accepted

**Context.** An update found by the daily check was offered on Home's Update
widget and in Settings (decision 037), but nothing said "look here" on the
way: the Settings widget looked the same with or without one. Other things
will want the same mark (a missed alert, a finished download), on whatever
kind of element leads to them.

**Decision.**
- **One badge, any element.** Providers set a flag; the host decides where
  the dot goes and draws it. The flags: `Content::badge` (a dot on its
  widget on Home and on its tab, so the way to its card is marked) and
  `Button::badge` (one button on a card). Where each dot goes is pure and
  in one place (`core/badges`). A widget's or tab's sits in its top-right
  corner **concentric with the corner's rounding** (decision 006): at the
  centre of the corner's arc, so the gap to the curved edge is the same all
  round, and it sits as far in as the corner is round. Only a corner
  tighter than the dot plus `badgeInset` falls back to that inset. A
  button's sits **on its round outline**, 45° up and right from its centre,
  not in the corner of its square frame. (The first version used a fixed
  inset and the frame's corner: off-centre against the rounding, and too
  close to the edge.)
  A button scrolled out of its list's window has none. The scene carries
  the centres (`Scene::badges`); the renderer draws a dot at each
  (`platform/badge_dots`), over the content, fading in and out.
- **Tokens:** `badgeSize` 8, `badgeInset` 7, `badgeColor` red: the colour
  every OS uses for "new", the same in every theme (a theme may override it).
- **The first user, updates:** while an update is offered (or its install
  failed, so it still needs doing), the **Update widget** on Home has a dot,
  and so does **Home's button** in the tab strip, so it's seen from any
  card. While installing, neither does: there's nothing for the user to do.
  (At first Settings' widget and its Update and restart row had the dots;
  the Update widget is where the news is, so they moved there.)
- **Home's button is for news only, not every dot.** `Content::badgeHome`
  asks for it, on top of `badge`; only the update sets it. Another widget's
  dot stays on that widget and never lights Home's button.
- The Update widget is stacked like the other small widgets (icon and
  version, then "Update"), without a square at its side, so it fits a
  narrow column.

**Consequences.** Placement in `tests/badge_tests.cpp`, the Settings flags
in `tests/settings_tests.cpp`. The look needs a Windows run. A future badge
is a flag on the content; nothing new in the renderer.
