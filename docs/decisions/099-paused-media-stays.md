# 099: Paused media stays while Windows has its session

Status: Accepted (amends 005)

**Context.** Decision 005 ended paused media after `mediaPausedLingerMs`
(5 min), through core activity expiry, on the grounds that browsers hold
paused sessions for hours. In use, the track vanishing while still paused in
its app was the surprise: you pause, come back, and the controls are gone
though the app (and Windows' own media flyout) still has it.

**Decision.** No expiry for media, paused or playing. What's shown follows
the Windows session alone: it ends when the session is gone (after the
`mediaSessionGoneGrace` of decision 074), or when the app closes it.

**Consequences.** The `mediaPausedLingerMs` token is gone. A browser tab
paused for hours stays on the island until the tab closes or its session
drops, same as in Windows' flyout. Tested in `tests/media_tests.cpp`.
