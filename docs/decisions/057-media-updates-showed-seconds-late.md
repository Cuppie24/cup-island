# 057: Media updates showed seconds late (two bugs)

Status: Accepted (fixed in 0.6.10 and the release after it); frozen after sleep (threads): 060

**Symptom.** Pausing, playing or skipping in Spotify sometimes took 2–5 s
to show on the island: the play/pause button, the progress bar, the
waveform dots and the title all stayed as they were, then changed at once.
It came and went on the same version. It looked like an animation delay
(the colour fade, the cover wait of 056) or Windows being slow; it was
neither.

**How it was found.** The log. "media: new track" was followed by "media:
cover shown" within 10–40 ms, so reads and drawing were fast once they
happened. But no line appeared on a pause at all, and when the playback
event was logged ("media: playback changed", added for this) nothing
followed it for seconds. So the delay was between the event and the read
reaching the screen, in our code.

**Bug 1: playback events never arrived.** `watchAllSessions` subscribed
each session's `PlaybackInfoChanged` on the objects `GetSessions()` returns
and kept only the C++/WinRT revokers. A revoker holds a *weak* reference to
its source, so nothing kept those objects alive: they were freed at once
and never raised the event. Track changes still worked, because
`MediaPropertiesChanged` is subscribed on `session_`, which is held. A
pause was only noticed at the app's next timeline update (Spotify: every
few seconds). Fix: each watcher holds its session
(`PlaybackWatcher { session, revoker }`).

**Bug 2: newer reads cancelled older ones.** Every event (playback,
properties, timeline) started `load()`, and after its `co_await
TryGetMediaPropertiesAsync()` a read was dropped if a newer one had been
started ("newest wins", a generation counter). Around a pause or a skip
Spotify sends a burst of timeline updates, faster than a read takes, so
every read was dropped, and nothing reached the screen until the burst
ended. How long a burst lasts varies, hence "sometimes". Fix: one read at
a time. An event during a read asks for one more after it, and the read in
flight is still shown, so the screen trails the latest event by at most
about two reads. A read of a session no longer shown is still dropped, and
one still running after `mediaReadStuckMs` (2 s) doesn't hold the next
back, so a hung call can't freeze the island.

**How to check it now.** The log times the whole path: "media: playback
changed" (the event), "media: paused" / "media: playing" (shown), and
"media: read took N ms" for any read over 250 ms. A pause should show the
first two lines milliseconds apart.

**Lessons.**
- A WinRT event subscription doesn't keep its source alive. Whatever
  raises an event we rely on must be held by us, not only by a revoker.
- "Newest wins" is wrong for a source that can send events faster than
  one read takes: it can starve every read. Coalesce instead (one in
  flight, one queued).
- When something on screen is late, log both ends (the event and what's
  shown) before guessing at the cause.
