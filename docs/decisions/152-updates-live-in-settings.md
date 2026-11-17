# 152: Updates live in Settings, marked by an accent dot

Status: Accepted (supersedes 038's Update widget and Home's dot; amends 036, 037, 091).
Core and Settings tested; the dots' look needs a Windows run.

**Context.** A found update had a widget of its own on Home, with a card
(Install and restart, What's new, Download when it failed), an alert once
per version, and a red dot on the widget and on Home's button. Settings'
Updates page offered the same install. The user wanted the widget gone, an
accent dot in its place, and What's new in Settings too.

**Decision.**
- **No widget, card or alert.** UpdateProvider only checks, keeps the
  offer and installs; Settings is told the offer (`UpdateStatus`, now with
  its release page, its installer and whether it's mandatory). A mandatory
  release's "Updating" alert stays: it installs without a click, so it says so.
- **The dot**: while an update waits (offered, or its install failed; not
  while installing), Settings' gear in the page row gets a dot in the
  theme's accent (`liveColor`), and so does Updates in Settings' sidebar.
  Red stays for "a setting you haven't seen" (decision 048); the accent says
  "something to do". It replaces the red one if both apply.
  `Content::badgeAccent` (replacing `badgeHome`: Home's button no longer gets
  a dot), `Tab::accent`, `Tile::accent`, `Scene::accentBadges`,
  `WindowPage::badgeAccent`. The island draws them as a second `BadgeDots`.
- **Settings › Updates** has what the card had: Update and restart (its
  detail says when a mandatory one installs by itself), **What's new** (the
  offer's release notes, else this version's; none in a dev build; opens
  the browser via `SettingsHooks::openPage`), and, after a failed install,
  **Download installer** to run by hand. What's new isn't tagged "New": it's
  a link, not a setting.

**Consequences.** `tests/whats_new_tests.cpp`, `tests/update_tests.cpp`
(the status instead of the widget), `tests/badge_tests.cpp`. The store's
old "announced" key is no longer read.
