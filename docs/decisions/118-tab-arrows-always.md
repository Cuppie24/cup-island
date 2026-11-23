# 118: The tab row's arrows show with one page, disabled

Status: Superseded by 165 (no arrows). Amended 109 (the arrows were shown only with somewhere to go). Core tested; the look on Windows needs a run.

**Context.** With one activity and Home's widgets, the page row showed Home's
button, one line and the pin, and no arrows; with a second activity the
arrows appeared and the row's middle shifted. The user asked for the arrows
always, disabled when there's only one page.

**Decision.** With any page in the row, ‹ and › are placed beside the dots
as before. With nowhere to go (one page, and it's the one shown) they're
drawn in `buttonDisabledColor` (`Icon::disabled`), not muted (they never
light), and the strip has no `previous` / `next` button: nothing to click,
hover or focus. Somewhere to go, they're the live arrows of decision 109.

**Consequences.** `Icon::disabled` is a general dim glyph, for any button
the host draws as an icon with nothing to do.
