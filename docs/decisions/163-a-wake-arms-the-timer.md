# 163: A provider's wake-up arms the timer when it's asked for

Status: Accepted. Registry tested on Linux; the media case needs a Windows run.

**Context.** Spotify paused and then quit: Windows' media controls dropped it
at once, but the island kept showing the song, as if playing, until it was
clicked. The media reader did see the session go. The presenter waits
`mediaSessionGoneGrace` before ending it (decision 074) and asks for that
with `Host::wakeAt`. But `wakeAt` only recorded the time. The app's one
timer is armed at the end of a dispatch, from `nextWake()`, and the
"session gone" report changes nothing on the island, so it causes no
dispatch. The wake waited for the next one: the click. It was already
due by then, so the song went at once, which made it look like the click
had ended it. Other providers ask for wakes from clicks and actions, which
dispatch anyway, so they never showed it.

**Decision.** `ProviderRegistry::onWakesChanged` runs whenever a provider
asks for a different wake-up (or cancels one), and the app re-arms its
timer from it (`armDeadline` in `main.cpp`). The same wake asked again
changes nothing and re-arms nothing.

**Consequences.** `tests/provider_tests.cpp` (`runWakeHookTests`). A wake
asked from posted work comes on time, whether or not the island changed.
