# 055: The blurred cover behind the island; music colours off by default

Status: Accepted (changes 052's default)

**Context.** Beyond tinting the island (decisions 051, 054), the most
striking "now playing" look is the cover itself, blurred, behind the
controls, as iOS and Spotify do. And how much the island changes with the
music is a matter of taste: someone who just installed Cup Island should
get their theme, not a colour show they didn't ask for.

**Decision.**
- **Blurred cover.** `core/cover_backdrop` blurs a cover once, when it's
  read: shrunk to `coverBackdropSize` (32 px) by averaging, then three
  box blurs each way (close to a Gaussian) of `coverBackdropBlur` (12%) of
  that size. Providers keep it with the art (`NowPlaying::backdrop`), and
  `nowPlayingContent` puts it in `Content::backdrop` while playing. The
  island shows the best-ranked activity's, with the tint's timing (at once;
  `coverTintLinger` after it's gone; `nextBackdrop`).
- **On screen** (`platform/backdrop_layer`): a sprite under the content,
  its small picture stretched over the island, clipped to the island's own
  outline (`islandGeometry`, shared with the background, so they can't
  disagree), at `coverBackdropOpacity` over the island's colour: [0.5 dark
  themes, 0.35 light ones], low enough for text to read. A new cover
  crossfades over the old over the colour fade's duration (decision 053).
  It's a still picture: nothing is blurred or redrawn while music plays.
  The pill has it; the Split bubble doesn't.
- **Three switches on Music colours, all off by default**: "Colours from
  music" (`covercolours`, now off unless saved "on"), "Two-colour gradient"
  (`covergradient`, decision 054; only switchable while the colours are on;
  off, a two-colour cover gives one colour) and "Blurred cover"
  (`coverbackdrop`, independent of the colours). The list's Music colours
  row says On if any is. The fade speed stays as it was.

**Consequences.** Tested in `tests/cover_tests.cpp` (the blur, its timing,
the content) and `tests/settings_new_tests.cpp` (the switches, their
defaults, saves and restart). People who had Colours from music on by
default before now have it off until they turn it on. The look needs a
Windows run; the strengths are tokens, live-tunable.
