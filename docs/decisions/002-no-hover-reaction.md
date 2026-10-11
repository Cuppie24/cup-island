# 002: No hover reaction on the pill; hover only on controls

Status: Accepted (revisit with PS §12 Q5)

**Context.** It's tempting to make the pill react when the cursor approaches.
But Expanded has real controls (media buttons, a seekable progress bar), and
with a mouse, controls that don't acknowledge the pointer feel dead.

**Decision.** The pill itself never reacts to hover. The real Dynamic Island
is touch-only, and the island sits at the top edge, where the cursor passes
constantly during unrelated work, so a proximity pulse would be noise.
Attention animations are event-driven instead (alerts, PS §3.4), so they
always mean something new happened.

Interactive *content* does react, because the pointer is already on it with
intent: a button shows its hover shape, and a progress bar shows its scrub
knob at the playback position. Which element is hovered comes from core
`hitTest` (the same hit areas a click uses), so the feedback never lies about
what a click would do.

**Consequences.** `PointerMoved` keeps Expanded open while the user is
reading it; the app also uses the pointer position for content hover. A
progress bar is a slider, not a button: it never squashes on press, it
follows the pointer while dragged and seeks on release. A small "peek" on
hover in Compact is still an open question (PS §12 Q5).
