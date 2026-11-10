# 089: Shortcuts come forward over Start and search

Status: Accepted

**Context.** With Start (or Windows search) open, Double Ctrl opened search's
card but its field didn't get the keys: typing still went to Start. Start's
flyout keeps the foreground from other apps while it's open. The old
`bringToFront` (decision 079) trusted SetForegroundWindow's answer, which can
be yes while the window only flashes, so its Alt-tap fallback never ran; and
an Alt tap doesn't close Start anyway. Home's and the activity's shortcuts
called SetForegroundWindow alone, so a double tap bound to them had the same
problem.

**Decision.** One `platform::bringToFront` (`platform/foreground`) for every
shortcut command:
- Success is what `GetForegroundWindow` says, not the call's answer.
- If Start or search is in front (by its host process:
  StartMenuExperienceHost, ShellExperienceHost, SearchHost/SearchApp/SearchUI,
  Windows 10 1809 to 11), it sends Esc first, as a person would, so the
  flyout closes. Only once per shortcut, and only with no modifier held:
  Ctrl+Esc would open Start again, Alt+Esc switch windows, Win+Esc close the
  magnifier. A chord's modifiers usually still are held when its hotkey
  arrives; the next try sends it once they're let go.
- Otherwise, mildest first: SetForegroundWindow; an Alt tap and again;
  AttachThreadInput to the window in front for a moment and again.
- Not in front yet (the flyout takes a moment to close): it tries again every
  `foregroundRetryMs`, at most `foregroundRetries` times (30 ms × 20), then
  logs who kept the foreground. Focus goes to the island once it's in front.

**Consequences.** Opening search (or Home) from Start closes Start, which is
what Start does for any app that comes forward. The island may take up to a
few frames to get the keys there; elsewhere it's at once, as before. Esc is
never sent to anything but Start or search, so the app behind doesn't see it.
