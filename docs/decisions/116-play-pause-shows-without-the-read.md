# 116: Play/pause shows without waiting for the properties' read

Status: Accepted (follows 057). Provider and presenter tested; the timing on Windows needs a run.

**Symptom.** Pressing ▶ (on the island or in Spotify) started the music,
but the island's button stayed ▶ for about 10 s, then turned ⏸.

**Cause.** Every change went through one path: an event asks for a full
read, whose first step, `TryGetMediaPropertiesAsync` (title, artist,
cover), can take seconds with Spotify. Decision 057 made reads one at a
time: an event during a read only asks for another after it. And a read
older than `mediaReadStuckMs` (2 s) was meant not to hold the next back,
but that was only checked when another event came. With a slow read in
flight at the moment of the pause, nothing showed until it ended or until
the app's next timeline update, which with Spotify can be 10 s or more away.

**Decision.**
- **Playback is read on its own, synchronously** (`readPlayback`: status,
  rate, timeline, which controls work, shuffle, repeat). On a playback or
  timeline event it's read at once and shown over the last full read
  (`MediaPresenter::showPlayback`): title and cover stay, no alert. Only
  for the session shown; another app's change waits for its full read.
- **The full read still follows**, as before, and corrects anything.
- **A read in flight that may hang re-reads at the stuck mark**: when an
  event is deferred behind a read, a re-read is scheduled for
  `mediaReadStuckMs` after the read began, not left to the next event.

**How to check it.** The log: "media: playback changed" then
"media: paused (from the playback event)" in the same millisecond or so.
