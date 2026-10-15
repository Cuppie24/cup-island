# 056: A track shows at once; its cover fades in

Status: Accepted (replaces 005's "art before alert")

**Context.** A new track was held back until its cover was read, up to
`mediaArtWaitMs` (1 s), so its alert carried the picture. Covers come
after the title (Chrome, often Spotify), so skipping a track felt slow:
the old title stayed for up to a second.

**Decision.**
- `ArtState` no longer holds anything back: a new track shows at once,
  its old cover dropped (it's wrong for it), with the placeholder until
  its own is read. Chrome's late picture is still re-read once
  `mediaArtRecheckMs` after the change.
- A changed picture crossfades from the old one (or the placeholder) over
  `mediaArtFadeMs` (400 ms; `fadeInSurface`): the element shows it at
  once, under a copy of the old picture fading out. Not new over old: the
  new picture's antialiased rounded edge let the old one's show through
  as a thin light rim until the fade ended. Any Image
  does this, not only media art.

- The track's alert is posted at once too, so it has no cover yet. When
  the cover is read, the presenter posts the same alert with
  `refreshOnly`: a showing or queued alert with its coalesceKey takes the
  new content, its time not extended; if it has gone, nothing shows.

**Consequences.** A track's alert can appear without its picture, which
fades in a moment later. Colours from music still follow the cover, so
they change when it arrives. Tested in `tests/media_tests.cpp` and `tests/scenarios/alerts.scenario`.
