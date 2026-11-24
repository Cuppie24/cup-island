# 178: The island is dragged to another monitor

Status: Accepted (replaces 023's Display setting; amends 026's "only the island's own monitor")

**Context.** The island's monitor was picked in Settings › Display
(decisions 023, 030), and a lifted island stopped at its monitor's edge
(decision 026). The user asked to drag it across instead: moving it along
the edge and moving it to another screen are one gesture to them, and a
settings page for it was a detour.

**Decision.**
- **Carried past the edge, it changes monitor.** While the island is lifted
  and the pointer is on another monitor, the island is on that monitor: the
  render layer moves there (as Settings' pick moved it), tokens are rescaled
  to its scaling (decision 017), and the lift goes on. It lands where it's
  let go, snapping by that monitor's width (decision 026).
- **Under the pointer, held on the screen.** Positions on the old monitor
  mean nothing on the new one (another origin, maybe another scale), so the
  pill's centre becomes the pointer's x (`IslandCarried`, `carryIsland`),
  clamped as any lift is. Crossing a side edge, that's flush with the edge
  it came in at. It isn't drawn half on each monitor: the render layer is
  one monitor wide (decision 026), and the two may differ in scale and
  height. So it jumps the bezel, by one pill's width.
- **By the pointer's monitor, not the pill's.** Monitors above or below
  work the same: the pointer goes down onto the lower one and the island is
  at its top edge, under the pointer.
- **Remembered as before.** The monitor's id goes where Settings' pick went
  (`system.settings/display`, `SettingsProvider::islandMovedTo`), so the app
  starts there, falls back to the main display while it's unplugged, and
  returns when it's back (decision 023). Each monitor keeps its own spot
  (decision 026); a carried island takes its new one where it lands.
- **Settings has no Display row or page.** A saved "" still means the main
  display, following Windows' primary; it stays so until the first drag to
  another monitor. After that it's always a particular monitor: there's no
  way back to "whichever is main", and none was asked for.

**Consequences.**
- Core: one event and one rule (`lift.h`), in `tests/scenarios/lift.scenario`.
- The app checks the pointer against its monitor's bounds on each lifted
  move (`carriedAcross`, one comparison); monitors are listed only on a crossing.
- The interaction window is put under the pointer on the new monitor
  (`setRenderOrigin`'s offset): left at its old offset it could sit on a
  third monitor, and its `WM_DPICHANGED` would rescale to that one.
- Keyboard-only users can't change the island's monitor any more.
- The Windows side (the layer moving mid-capture, glass re-capturing its new
  monitor, mixed scalings) compiles in CI and needs a run on two monitors.
