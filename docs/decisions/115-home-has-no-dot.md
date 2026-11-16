# 115: Home has no dot in the page row; its button is the way there

Status: Accepted (amends 109: the page row's dots; 033: Ctrl+Tab). Core tested; the look on Windows needs a run.

**Context.** The page row had a dot for Home first, then one per activity,
and Home's button at the left edge did the same thing as Home's dot. The
user asked for Home to be reached only by its button.

**Decision.**
- **The dots are the activities only**, in start order. On Home none is
  shown as a line; the ‹ › arrows take it to the first or the last activity.
  With nothing running there are no dots and no arrows: Home's button, the
  pin and any panel buttons (decision 113).
- **Home's button is a keyboard stop** now that no dot stands in for it.
  Its badge (a widget flagged `badgeHome`) was already on it.
- **Ctrl+Tab cycles the activities**, round from the last to the first; from
  Home or a dormant card, the first (Ctrl+Shift+Tab: the last). Nothing with
  none running.

**Consequences.** Home's button is the one way to Home from a card, beside
Esc-then-click and the idle pill.
