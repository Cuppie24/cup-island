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
  in one place (`core/badges`): a widget's or tab's in its top-right corner,
  `badgeInset` in; a button's on its top-right corner, like an app icon's.
  A button scrolled out of its list's window has none. The scene carries
  the centres (`Scene::badges`); the renderer draws a dot at each
  (`platform/badge_dots`), over the content, fading in and out.
- **Tokens:** `badgeSize` 8, `badgeInset` 7, `badgeColor` red: the colour
  every OS uses for "new", the same in every theme (a theme may override it).
- **The first user, updates:** while an update is offered (or its install
  failed, so it still needs doing), the Settings widget has a dot, and so
  does the Update and restart row's button it leads to. While installing,
  neither does: there's nothing for the user to do.

**Consequences.** Placement in `tests/badge_tests.cpp`, the Settings flags
in `tests/settings_tests.cpp`. The look needs a Windows run. A future badge
is a flag on the content; nothing new in the renderer.
