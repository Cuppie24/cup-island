# 183: ▶ asks to play, ⏸ asks to pause

Status: Accepted (follows 182). The cause below isn't proven: the log line it adds tells. Windows-only code: needs a run on Windows.

**Symptom.** After the PC slept, the island's ▶ did nothing. Once the song
was played by hand, the island showed it at once and its buttons worked again.

**What the log shows.** Chrome (YouTube) on this PC drops its media session
when the screen goes off, and adds it back, paused, when the screen comes on
(11:52, 12:22, and 13:48 after the sleep). After 13:48 the island read it
right: paused, ▶. Nothing came of the ▶ presses: no `playback changed` until
it was played by hand at 13:54:28. From then on every press showed within
milliseconds. The island logged neither its presses nor what came of them.

**Cause (likely).** The button asked Windows to *toggle*
(`TryTogglePlayPauseAsync`). Which of play or pause a toggle means is
Windows' and the app's to work out, from what they each think the state is.
A session just re-added after a sleep is where the two can disagree: a toggle
that comes out as "pause" on something already paused does nothing.

**Decision.**
- **The button asks for what it shows** (`SmtcReader::playPause`): ▶ asks to
  play (`TryPlayAsync`), ⏸ to pause (`TryPauseAsync`), going by the state the
  island last showed. An app that refuses the one asked for gets the toggle,
  as before.
- **The ask is logged, and what came of it:** `media: play asked of Chrome`;
  then, only if it wasn't taken, `refused it, took the toggle`, `refused it,
  and the toggle too`, or `couldn't be asked (0x...)`.

**How to check it.** Leave a paused video, let the PC sleep, wake it, press
▶ on the island. It plays. If it doesn't, the log says which: a `refused` or
`couldn't be asked` line is Windows saying no; `play asked of Chrome` with
no `playback changed` after it means Windows passed it on and Chrome did
nothing with it (a tab Chrome put to sleep), which the island can't change.
