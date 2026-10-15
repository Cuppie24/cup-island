# 058: A pause recolours at once; only a coming cover waits

Status: Accepted (amends 051's linger)

**Context.** With Colours from music on, playing faded to the cover's
colours at once, but pausing started the fade back to the theme
`coverTintLingerMs` (1.5 s) late. Every way the colour could go away
waited, because the wait was there for one case: between two tracks, so
skipping didn't flash the theme. Since a track shows before its cover is
read (056), that case is "a new track playing, its cover not read yet",
which the media content can say.

**Decision.** `Content::tintComing`: set while media is playing without a
cover yet. `wantedTint` returns the best-ranked asking activity's colour
or that it's coming (`WantedTint`), and `nextTint`:
- a colour asked for: shows at once (as before);
- one coming: the colour shown stays up to `coverTintLingerMs`, then the
  theme if it never came;
- neither (paused, stopped, gone, the setting off): the theme at once.

**Consequences.** Pause and play now start their fades together. An app
that reports a brief pause between tracks would show the theme for that
moment; Spotify's log shows none. Tested in `tests/cover_tests.cpp`.
