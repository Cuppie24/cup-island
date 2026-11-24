# 177: Media's card doesn't close when left alone

Status: Accepted (amends 062)

**Context.** Expanded closes after `expandedIdleTimeout` without the
pointer moving (TR §5). Decision 062 kept that for media, as "a glance".
In use the player is kept open like Pomodoro's timer, to watch the track
and reach the controls, and it kept closing. The user asked for it to stay.

**Decision.** Media's cards set `Content::noIdleTimeout` (decision 062):
now playing, and "Nothing playing", so the card doesn't close under the
user when the music ends. Esc, a click outside the island and the page row
still close or leave it. Other cards keep the timeout.

**Consequences.** The demo player, which shows the same content, stays
too. Tested in `tests/media_tests.cpp`.
