# 158: Nothing escapes a fire_and_forget (the island vanished when Spotify quit)

Status: Accepted. Needs a Windows run: quit Spotify while it plays, a few times.

**Symptom.** The island disappeared with nothing in the log: no `CRASH:`,
no dump. The next start said `PREVIOUS RUN ENDED UNEXPECTEDLY` and then
`Windows has no crash record for it`. Twice in one evening (0.12.18), both
times just after `media: playback changed`: once as Windows' main media
moved from Chrome to Spotify, once as Spotify was quit (seven playback
events in one millisecond; the next start found `0 session(s)`).

**Cause.** `SmtcReader::load()` read the session inside a `try`, then went
on outside it: `SourceAppUserModelId()`, `GetSessions()`,
`properties.Thumbnail()`, `AlbumTitle()`. Each is a call into another
process. When the app closes its session mid-read, one of them throws.
`load()` is a `winrt::fire_and_forget`, and C++/WinRT ends the process at
once when an exception leaves one (`winrt::terminate()`, a fail-fast with
error context). That is not `std::terminate`: none of the watchdog's
handlers (decision 013) run, so there was no log line and no dump.

Windows does record a fail-fast (Application Error 1000), but a few
seconds after the process ends. A quick restart looked before that, once,
and reported no record.

**Decision.**
- The rest of a read is `showRead()`, called inside `load()`'s `try`: a
  session that went away mid-read is a failed read (logged, retried a few
  times), not a crash.
- A `fire_and_forget` catches everything that can leave it. `decodeArt`
  catches `std::exception` too; the update check's and the service link's
  callbacks run through `platform::guardedTask`.
- `crash_report.cpp` looks for Windows' record for about 30 s (every 2 s)
  before saying there is none.

**Rule.** In a `winrt::fire_and_forget`, every statement that can throw is
inside a `try`, or in a function called through a guard. "Can throw"
includes any call on a session, a manager or a property object, after the
first `co_await` too.
