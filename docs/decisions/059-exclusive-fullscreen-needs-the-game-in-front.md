# 059: Exclusive fullscreen hides the island only with the game in front

Status: Accepted (refines TR §9.2.3)

**Context.** After playing Valorant in exclusive fullscreen, the island
stayed hidden: alt-tabbed to other windows, after closing the game, and
even after restarting Cup Island (the log: `Idle -> Hidden (fullscreen)`
right at start, shown again only 20 minutes later). Two causes:
- `SHQueryUserNotificationState` reporting `QUNS_RUNNING_D3D_FULL_SCREEN`
  hid the island whatever window was in front, on any monitor. Windows
  keeps reporting it while the game is alt-tabbed away (it stays
  exclusive, minimized) and for a while as it closes.
- The check only ran on window events (foreground, minimize, move). If the
  flag was still up at the last event, nothing looked again, so the island
  stayed hidden until the next window switch. A restart's first check saw
  the same stale flag.

**Decision.**
- The exclusive flag counts only with a real app in front on the island's
  monitor: not the desktop, taskbar or Alt-Tab, not minimized, not on
  another monitor (TR §9.2.4), and not "no foreground window". Then it
  hides, whatever the window's bounds say.
- While hidden, the watcher checks again every `fullscreenRecheckMs`
  (2 s), so a flag that clears without any window event shows the island.
  Shown, there's no timer: idle still costs nothing.
- The hide line now names the app and how:
  `fullscreen app detected: hiding (VALORANT-Win64-Shipping.exe (class …), exclusive fullscreen)`.

**Consequences.** Rules in `core/fullscreen`, tested in
`tests/fullscreen_tests.cpp`. The watcher and its timer need a Windows run.
`describeWindow` moved from `stay_on_top` to `platform/window_name`, shared
by both logs.
