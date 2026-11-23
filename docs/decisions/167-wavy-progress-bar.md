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
  `progressWaveAmplitude` 3 to each side, the bar's thickness). Over the
  last `progressWaveTaper` (26) before its head it straightens out, so it
  meets the head on the bar's line. The rest is the straight track.
- Playing, its ripples run towards the head, a wave every
  `progressWavePeriod` (1500 ms). Not playing, it lies flat over
  `progressWaveSettle` (450 ms): a plain bar, at its full thickness. With
  reduced motion it rises but doesn't run.
- A seekable wavy bar's knob is an upright pill at the head
  (`progressPinWidth` 4 by `progressPinHeight` 12, in the knob's colour),
  always there, `progressPinGrow` (1.3) times bigger under the pointer or
  dragged. The track starts `progressPinGap` (3) past it. A plain bar's knob
  is as it was: a circle, only under the pointer.
- Its row is at least the wave's height, and the grown knob's
  (`waveHeight`); a seekable bar's click height already is, so media's card
  doesn't change size.
- While a bar is dragged, its row's time labels show the time under the
  pointer, not playback's (`scrubbedTimes`, core, applied to every scene
  laid out during the drag). Any seekable bar's, wavy or not. Let go, they
  show playback's again until the app reports the seek.

**How.** The line is a path (`CompositionPathGeometry`, Windows 10 1809, in
`platform/progress_wave`) as long as the bar, hanging from its head: its
shape's offset is an expression of the fill's live width, so playback and
a drag move it with no code of their own. The fill's rectangle stays,
unseen: its width is still what animates, and the knob and the track read
it.
- **Ripples** are the path itself changing: twelve paths, one per step of a
  ripple, each straightening into the head, in one `PathKeyFrameAnimation`.
- **Which of them shows** isn't the animation's own time. The animation is
  held still (its `AnimationController` paused) and its progress is an
  expression: a clock (a scalar looping 0 to 1 once a ripple) less the
  head's distance, in waves, from where it was when the wave rose. So
  while the head is dragged the ripples stay where they are on the bar and
  keep running, and only the straightening follows the head.
- **Rising** is the first stretch of the same animation, flat to the first
  phase, driven by a scalar eased 0 to 1 over `progressWaveSettle` (450 ms);
  the clock starts, at 0, as it ends.
- **Lying down** is a path animation of its own with both ends given
  (decision 008): from the line as it is at that moment to flat. Composition
  can't be asked what it's showing (decision 001), so `ripple()` works that
  line out as the expression does, from when the wave rose and where the
  head is, and makes that one path.
`ripple()` remembers what it was told, so the card's once-a-second layout
doesn't restart anything. The points are core's (`wavePoints`), tested
there. Its start shows through a window with round ends (a geometric
clip). If the wave can't be made, or its animation driven, it's logged
(`progress: ...`) and the bar is plain or the wave stays flat.

**Tried on Windows and turned down.**
- One fixed path sliding under a clip, squashed flat by its Y scale: the
  stroke thinned with the scale whatever `IsStrokeNonScaling` said, a
  hairline beside the track, and a sliding path can't straighten at a head
  that moves.
- Ripples running back from the head; then towards it again, as now.
- The wave's phases played by the animation's own time, with the line
  hanging from the head: dragged, the whole wave moved with the pointer.
- Rising and lying down as path animations with only their end given, the
  loop started when the rise ended: neither read as the wave growing from,
  or straightening into, a line.
- Ends cut straight by the clip: they looked cropped.
- At the head: an upright pin fixed on the line while the wave moved up and
  down against it; a round one riding the wave's end, which bobbed; a cap
  the line's own thickness riding it, with a knob only under the pointer.
  With the wave straight at its head, a knob fixed on the line has nothing
  to move against.

**Consequences.** Tested in `tests/item_tests.cpp` (height, the line: its
taper, its points, which way it runs), `tests/content_tests.cpp` (a drag's
times) and `tests/media_tests.cpp`. How it looks and moves needs a Windows
run; the numbers are live tokens.
