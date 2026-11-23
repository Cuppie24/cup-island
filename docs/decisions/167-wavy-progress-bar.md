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
still what animates, and the knob and the track read it.
`ripple()` remembers what it was told, so the card's once-a-second layout
doesn't restart the run. The line's points are core's (`wavePoints`),
tested there. If the path can't be made, it's logged and the bar is plain.

- Neither end is a straight cut (the first build's were, and looked
  cropped). The head has a round cap: a dot as wide as the line is thick,
  whose centre is an expression of the line's slide and of how flat it
  lies, the same sine the path was made from, so it stays on the line's end
  as that rides up and down. The start shows through a window with round
  ends (a geometric clip), so it's cut along a curve. The hover knob rides
  the head too, and it and the cap are in a layer over the wave, so the
  knob covers it.
- **Not taken.** A pin at the head, bigger than the line: upright and fixed
  on the bar's line, the wave slid up and down against it; round and riding
  the head, it bobbed. Both were tried on Windows and turned down.

- **Paused, the bar's own straight fill shows.** Lying flat was the line's
  Y scale going to almost nothing with a non-scaling stroke; on a build the
  stroke thinned with it, a hairline beside the 4 px track. So as the wave
  sinks it also fades out, and the fill's rectangle, there all along for
  its width, thickens from nothing to the bar's height (`ripple`).
- **The knob.** Under the pointer it rides the wave's head; dragged, it's on
  the bar's line, where the pointer aims (`rideHead`: a scalar the knob's
  expression multiplies the swing by).
- **A drag moves the times.** While a bar is dragged, its row's time labels
  show the time under the pointer, not playback's (`scrubbedTimes`, core,
  applied to every scene laid out during the drag). Any seekable bar's, wavy
  or not. Let go, they show playback's again until the app reports the seek.

**Consequences.** Paused, the bar is a plain one with round ends. Tested in
`tests/item_tests.cpp` (height, the line) and `tests/media_tests.cpp`.
How it looks and moves needs a Windows run; the four numbers are live tokens.
