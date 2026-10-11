# 004: Content model: a subset of PS §5, slots of rows

Status: Accepted

**Context.** PS §5 describes slots holding elements with `hstack`/`vstack`
nesting, ids for matched geometry, and accent colours. Media needed real
content now, and full nesting means a general layout engine.

**Decision.** Each slot is a vertical stack of `Row`s, and a row is a
horizontal list of items (one level, no nesting). Items: Text, Image, Icon,
Button, Progress, TimeLabel, Waveform (decision 010 adds a ring shape for
Progress, Steps, and an accent colour). Text and Progress stretch; the rest
have token-defined sizes. Alerts get two slots (`alertLeading` +
`alertCenter`) instead of PS's single `alert` slot, so art and text sit side
by side like the other presentations.

Compact has a third slot, `compactCenter`, between the two squares (e.g. the
track name). `Text::marquee` makes a string that doesn't fit "swim"
(pause, scroll, loop, soft edges) instead of ending in "…"; the renderer
measures it, and the loop runs on the compositor. `Row::loose` asks for
wider gaps (controls), and `Row::leading` / `Row::trailing` pin items to the row's edges
while the rest stay centred on the full width (shuffle and repeat in the bottom corners). `expandedTrailing` (PS §5.1
`expanded.trailing`) is a button-sized square in the top-right corner, used for
the media source switch; the centre slot only narrows when it's filled. `Button::on`
marks a toggle: lit with a dot when on, dimmed when off. A paused `Waveform` rests as still dots instead of
being swapped for a pause icon, so play/pause is a calm in-place change.

Time-based content stays host-driven (PS §4): a Progress bar animates on
the compositor from its Timeline, and TimeLabels are re-formatted by
`layout()` once per second only while visible (`sceneDeadline`).

**Consequences.** Layout is ~150 lines of pure, tested code
(`core/layout.cpp`). Deferred: nested stacks, matched geometry (elements
crossfade between modes instead of moving), accent colours, `minimal` slot
rendering (needs the Split bubble). Each can be added without breaking
existing providers, since it only adds fields.
