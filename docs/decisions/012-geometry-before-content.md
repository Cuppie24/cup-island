# 012: Start the morph before building content

Status: Proposed (needs confirming on a real build)

**Context.** Clicking to expand or collapse felt laggy on mouse-up but not
on mouse-down. Mouse-down only starts a Scale animation. Mouse-up runs
`reduce → layout → apply` in one handler, and `apply` built the new content
there too: a CompositionDrawingSurface per new element, plus Direct2D
drawing (text, glyphs, album art). Composition commits property changes
only after the handler returns, so the new Size, and with it the morph,
waited behind all that drawing. How long that takes depends on how many
elements change, which is why it was only "sometimes".

**Decision.** `PillRenderer::apply` sets geometry (size, offset, tabs,
tiles, bubble) right away and queues the content on the thread's
DispatcherQueue at Low priority, after the compositor's commit work. Only the
latest scene is kept, so a burst of applies (pointer moves) builds once.
New content already waits `contentFadeDelayMs` before fading in, so
building it a frame later doesn't show.

**Consequences.** Element visuals trail the scene by one queue turn. Hover,
press and scrub on an element that was created in that same turn find
nothing (a click faster than a frame). Debug builds print
`content took N ms` when a build takes over 8 ms, to check whether this is
where the lag was. If the lag remains, the next suspect is the cost of
drawing itself (cache art bitmaps, reuse surfaces across modes).
