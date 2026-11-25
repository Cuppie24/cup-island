# 181: A second object for a media session read "paused" while it played

Status: Accepted (amends 005, follows 116); not the whole cause: the button still stuck in 0.14.4, see 182. Windows-only code: built on Linux only as far as its pure rule; needs a run on Windows.

**Symptom.** Spotify playing "Call It Fate, Call It Karma", started from the
island's buttons: the card's button stayed ▶, while Windows' own media
flyout showed ⏸ for Spotify. It corrected itself, much later. Chrome and
Media Player, in the same run, showed play and pause at once.

**How it was found.** The log, with Spotify, Chrome and Media Player open:

    22:28:27.426 media: playback changed
    22:28:27.426 media: playing (from the playback event)
    22:28:35.007 media: manual pick dropped, Windows moved its main media
    22:28:35.007 media: Windows' main media is Spotify
    22:28:35.007 media: showing Spotify
    22:28:35.007 media: paused

Spotify was already shown (picked with ⇄), so "showing Spotify" again means
`GetCurrentSession()` returned a different object for the same session than
the one `GetSessions()` had given us. The island switched to that object, and
its playback info said "paused", while Windows' own flyout showed ⏸ for
Spotify. So Windows knew; that object didn't.

**Cause.** Windows can hand out two objects for one session (005 already
matched picks by app id for this). Apparently each keeps its own copy of the
session's state, and the second one's playback stayed as it was. Why Windows
keeps one up to date and not the other isn't documented, and couldn't be
seen from here. What can be made sure of: the object the island reads when
an event arrives is the one the events come from. Before this, it was whatever
`GetCurrentSession()` returned, and its own playback events were never
subscribed (only every *watched* session's were).

**Decision.**
- **The object read is the watched one** (`SmtcReader::watchedFor`, rule in
  `session_choice::watchedSession`, tested): whichever session is chosen
  (Windows' main media, the ⇄ pick, the first one), the island reads the
  object whose `PlaybackInfoChanged` it holds, the same object if it is one,
  else the only watched session of its app. With two sessions of one app (two
  browser profiles) neither can be told for sure, and Windows' object is kept.
- **...and what's read raises its own events.** A shown session that isn't
  a watched object (that last case) gets its own `PlaybackInfoChanged`, which
  shows its play/pause at once (116).
- **The log names the app** on every `media: playback changed (Spotify)`,
  and says once per app when a second object was swapped for the watched one:
  `media: Windows handed out a second object for Spotify: reading the watched one`.

**How to check it.** Spotify and another media app open. Pick Spotify with
⇄, press ▶ on the island, then let Windows make Spotify its main media (wait,
or use Windows' flyout). The button stays ⏸ throughout, as in Windows' flyout.
If the second object shows up, the log has the "second object" line and no
`media: paused` while it plays.

**Lessons.**
- Two WinRT objects for the same thing can disagree. Read through the object
  whose events you hold.
- Log who an event came from: "playback changed" with no app name couldn't
  say whether Spotify had sent anything at all.
