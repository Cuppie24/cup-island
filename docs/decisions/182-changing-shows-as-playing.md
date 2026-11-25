# 182: A session that's "Changing" shows as playing

Status: Accepted (follows 181). Deduced from two logs, not yet seen in one: the log line below settles it. Windows-only code: needs a run on Windows.

**Symptom.** Still after 181 (0.14.4): Spotify playing, the island's button
stuck on ▶, while Windows' own media flyout showed ⏸. It came right by
itself, seconds to minutes later.

**What the logs show.** In 0.14.3, from 22:28:35 to 22:30:22, every read of
Spotify said "not playing": through the object `GetCurrentSession()` gave,
through the one `GetSessions()` gave (the ⇄ button, 22:28:47 and on), on
Spotify's own playback events and on others'. So 181's second object was
not the only cause. Each stuck spell ended the same way, with a late event
from Spotify: `playing (from the playback event)` at 22:28:27, 22:30:22 and
23:22:37.

**Cause (deduced).** The island read `PlaybackStatus() == Playing`. SMTC has
six statuses: Closed, Opened, Changing, Stopped, Playing, Paused. If Windows'
flyout and the island read the same status, the flyout showing ⏸ where the
island showed ▶ means it was neither Playing nor Paused. `Changing` (between
two states: loading, buffering, the next track) is the one a playing app can
be left in until its next real change. That fits every stuck spell, and
that Chrome and Media Player never stuck. The logs couldn't confirm it: they
only said "playing" or "paused".

**Decision.**
- **Changing shows as playing** (`showsPlaying`, pure and tested; Windows'
  enum mapped by `fromWindows` in `smtc_reading`). Playing too, as before.
  Paused, Stopped, Opened and Closed show ▶. An app passing through Changing
  on its way to a pause shows ⏸ for that moment; one between two tracks no
  longer flickers ▶.
- **The log says the status itself** when it's neither Playing nor Paused,
  and once more when it's back to one of them:
  `media: Spotify reports Changing (shown as playing)`. Before, only
  "playing" and "paused" were logged, which couldn't show this at all.

**How to check it.** Play Spotify until the old bug would have shown ▶
while it plays. The button shows ⏸, and the log has `reports Changing`.
If it still shows ▶ and the log has no such line, the status really is
Paused while Spotify plays: then the cause is elsewhere (a stale read), and
the log's `playback changed (Spotify)` lines time it.
