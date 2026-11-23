# 167: A progress bar can be wavy

Status: Accepted. Needs a Windows build and run: the platform code is
uncompiled.

**Context.** Media's bar was the one part of its card that looked the same
playing and paused. A wave along the played part, moving while music plays,
says "playing" where the eye already is, and needs no audio data (Windows
gives none for a track).

**Decision.** `Progress::wavy`, an option of the host's bar, so any
provider's bar can have it; media's does.
- The filled part is a sine line (`progressWaveLength` 28 long, swinging
  `progressWaveAmplitude` 3 to each side, the bar's thickness). The rest is
  the straight track, starting where the wave ends.
- Playing, it runs back from its head, a wave every `progressWavePeriod`
  (1500 ms): the head leads and the ripples trail it. (Run towards the head
  first; on Windows that read as backwards.) Not playing, it lies flat over `progressWaveSettle` (300 ms):
  a plain bar. With reduced motion it rises but doesn't run.
- Its row is at least the wave's height (`waveHeight`); a seekable bar's
  click height already is, so media's card doesn't change size.

**How.** The wave is a path (`CompositionPathGeometry`, Windows 10 1809, in
`platform/progress_wave`), longer than the bar by one wave; sliding it one
wave's length and starting over is the motion. It's in a child visual
clipped at the fill's live width, so playback and scrubbing move its end
with no code of their own. The fill's rectangle stays, unseen: its width is
still what animates, and the knob and the track read it. Lying flat is the
line's Y scale going to almost nothing with a non-scaling stroke.
`ripple()` remembers what it was told, so the card's once-a-second layout
doesn't restart the run. The line's points are core's (`wavePoints`),
tested there. If the path can't be made, it's logged and the bar is plain.

- Neither end is a straight cut (the first build's were, and looked
  cropped). The head ends in a pin: the bar's thickness wide,
  `progressWavePinHeight` (12) tall, riding the fill's end, so the wave meets
  it wherever in its swing it is. The start shows through a window with
  round ends (a geometric clip), so it's cut along a curve.

**Consequences.** Paused, the bar is a plain one with the pin at its head.
The wave sits over the knob's left half while scrubbing. Tested in
`tests/item_tests.cpp` (height, the line) and `tests/media_tests.cpp`.
How it looks and moves needs a Windows run; the four numbers are live tokens.
