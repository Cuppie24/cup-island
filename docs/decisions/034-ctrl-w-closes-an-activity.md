# 034: Ctrl+W asks the activity's provider to close it

Status: Accepted

**Context.** With the keyboard able to reach every card (031, 033), there
was no key to be done with an activity. Closing means something different
for each one: a Pomodoro session can be stopped, but a misclick shouldn't
lose it; a media player's activity is Windows' own media session, which
the island doesn't own and can't end.

**Decision.**
- **Ctrl+W on an activity's card asks its provider** (`Provider::
  onCloseRequested`, routed by `ProviderRegistry::closeRequested`). The
  island ends nothing itself. The provider decides:
  - end it (`Host::endActivity`),
  - ask first (`Host::ask`, decision 020), and end it once confirmed,
  - or ignore it.
- **Ignoring is the default.** A provider that doesn't handle it keeps its
  activity, with no dialog and no sign anything happened. A provider owns
  its activity's lifecycle (PS §4), so nothing ends behind its back, and
  one written before this key existed behaves as it did.
- **Pomodoro** asks its stop question (the stop button's), then stops the
  session; its card stays, dormant. **Media** (the real player and the demo
  one) ignores it: the activity goes when the music does.
- Only on a live activity's open card (`shownActivity`): Home and a
  dormant panel's card (Settings) have nothing to close. It's a key on the
  island, like Esc, not a system-wide shortcut: Ctrl+W closes tabs in
  every other app.

**Consequences.** Tested in `tests/pomodoro_tests.cpp` (asks, then stops;
nothing on Home or a dormant card) and `tests/provider_tests.cpp` (a
provider that doesn't handle it keeps its activity). Settings' "On the
island" list gains it; that list now scrolls (decision 033).
