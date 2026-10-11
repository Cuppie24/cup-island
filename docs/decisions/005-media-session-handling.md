# 005: How the media provider handles Windows media sessions

Status: Accepted

**Context.** Windows exposes every app that plays media as an SMTC session
(TR §6). Several can exist at once, they appear and vanish freely, they report
art and timelines unreliably, and their events arrive on background threads.

**Decision.**
- **Which session:** a manual pick from the ⇄ button if that session still
  exists, else Windows' current session, else the first one. The pick is
  stored as the session object, not the app id, because every Chrome tab
  shares one app id.
- **Threading:** event handlers only post to the UI thread. All reads happen
  in coroutines started on the UI thread, which resume there. A generation
  counter drops stale reads, and a shared `alive` flag makes callbacks after
  `stop()` no-ops.
- **Art before alert:** on a new track the art is decoded (≤ 128 px) *before*
  the snapshot is shown, so the track alert carries its picture. Apps that
  send the title first and art later are covered: the art key includes
  "has thumbnail".
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
