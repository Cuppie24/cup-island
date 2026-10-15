# 005: How the media provider handles Windows media sessions

Status: Accepted; a track no longer waits for its cover (056)

**Context.** Windows exposes every app that plays media as an SMTC session
(TR §6). Several can exist at once, they appear and vanish freely, they report
art and timelines unreliably, and their events arrive on background threads.

**Decision.**
- **Which session:** Windows' current session (its "main media", the one
  the media keys control), else the first one. A manual pick from ⇄ overrides
  it until that session goes away or Windows moves its main media to another
  app. The pick is matched by session object first (every Chrome tab shares
  one app id), then by app id, because Windows can hand out a new object for
  the same session. "Windows moved its main media" compares app ids and
  ignores moments with no main media. Matching by object alone made the
  first click on ⇄ sometimes do nothing (seen mostly with Spotify). The rules
  are in `session_choice` and tested.
- **Noticing Windows' switch:** `CurrentSessionChanged` doesn't fire reliably
  when another app starts playing, so every session's PlaybackInfoChanged
  also triggers a re-check, plus one more `mediaSessionRecheckMs` (300 ms)
  later, because Windows can update its current session just after the
  playback event.
- **Keep watched sessions alive:** every session's PlaybackInfoChanged is
  watched, and each watcher holds its session object. A C++/WinRT revoker
  holds only a weak reference: without the strong one the object was freed
  at once and never raised the event, so a pause (in an app whose
  properties weren't changing) showed only at its next timeline update,
  ~5 s later with Spotify.
- **One read at a time:** an event during a read asks for one more after
  it; the read in flight is still shown. It used to be "newest wins" (a
  generation counter dropped the older read), but Spotify sends timeline
  updates faster than a read takes around a pause or skip, so every read
  was dropped and nothing showed until it went quiet, seconds later. Reads
  over 250 ms and play/pause as shown are logged, to time this. A read
  still running after `mediaReadStuckMs` (2 s) doesn't hold the next back.
- **Threading:** event handlers only post to the UI thread. All reads happen
  in coroutines started on the UI thread, which resume there. A read of a
  session no longer shown is dropped, and a shared `alive` flag makes callbacks after
  `stop()` no-ops.
- **Art before alert:** on a new track the art is decoded (≤ 128 px) *before*
  the snapshot is shown, so the track alert carries its picture, but only for
  `mediaArtWaitMs` (1 s): a thumbnail that never loads (seen with Chrome)
  once hid the track entirely. The cover is decoded again on every
  properties update and once more `mediaArtRecheckMs` after a track change,
  because Chrome reports all its tabs through one session and sends a tab's
  title before its picture: keyed on the title alone, the previous tab's
  cover stuck. Only the latest decode counts; identical pixels change
  nothing. Rules in `ArtState`, tested.
- **Failed reads** (an app mid-switch) are retried after
  `mediaSessionRecheckMs`, up to `mediaReadRetries` times in a row, since
  no further event may come.
- **Art opens the app:** clicking the expanded art brings the session's app
  forward (`app_activation`). It looks for the app's window by its taskbar
  app id, else by exe name ("Spotify.exe", "Chrome", "MSEdge"), preferring a
  window whose title shows the media (a browser window whose active tab is
  playing). Store apps with no window are activated through Windows; a
  desktop app with no window (Spotify in the tray) is started again, which
  shows the running one. Browsers: SMTC doesn't say which tab is playing,
  so if no window's title shows the media, the app's tab strips are read
  through UI Automation (`browser_tabs`, skipping web page content) and the
  tab whose name contains the title is selected, preferring one marked as
  playing audio (`tab_match`, tested). It's a guess by title: a renamed tab
  or two tabs with the same title can miss, and then the window is just
  raised. The search runs on a background thread (it calls into the browser).
  Hovering the art dims it and springs in an "open" glyph. Apps whose session id names neither (Firefox uses
  a hash) may not be found; that's logged rather than guessed, since
  launching a browser by id would open a new window.
- **Alerts:** only for a track change within the same app. Never on
  startup, when an app appears, or when switching apps with ⇄.
- **Paused media lingers** for `mediaPausedLingerMs` (5 min), using core
  activity expiry, then ends. iPhone keeps paused music around similarly.
  Browsers can hold paused sessions for hours, so "forever" would be noise.
- **Timeline noise:** a re-sent timeline within 750 ms of where the current
  one says playback is gets ignored, so frequent updates don't republish or
  re-arm the progress animation.
- **EXIF orientation is ignored** for art, which practically never has it.
  This keeps the decoded size equal to the requested size.

**Consequences.** All decisions except reading Windows live in the pure
`MediaPresenter`, tested on Linux (`tests/media_tests.cpp`). The WinRT side
(`smtc_media_provider`, `smtc_reading`) can only be checked on Windows.
