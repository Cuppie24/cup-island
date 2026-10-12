# 028: Providers keep their data in the host's store

Status: Accepted

**Context.** Pomodoro (to-dos, its session) and Settings (theme, display)
saved through load/save functions `main.cpp` handed each of them, one file
name per value. A plugin over the pipe (PS §8) can never be handed
functions, and PS §1 says built-ins use only what plugins can. Every new
value meant more code in `main.cpp`; writes overwrote the file in place, so
a crash mid-write could lose the to-do list. Pomodoro also needed the wall
clock, which only the app can read.

**Decision.**
- `Host::load(key)`, `Host::save(key, text)`, `Host::wallClock()`. A key
  is a plain name (letters, digits, '-', '_'); every provider's keys are its
  own (the registry adds its id). Text in, text out: what's saved and in
  which format stays the provider's (`TodoList::save`, `saveSession`). The
  host never parses it, so a format change never touches the host.
- The registry is handed a `Store` (load, save, wall clock). Without one it
  keeps values in memory and uses its own clock: tests need nothing.
- The app's store is `FileStore` (`providers/file_store`, portable, tested
  on Linux): `%LOCALAPPDATA%\CupIsland\store\<provider id>\<key>.txt`.
  Writes go to a side file that's then renamed over the old one, so a
  crash leaves the old text, never half of each. Saving "" deletes the file.
- Files from before the store (`todos.txt`, `theme.txt`, `display.txt`)
  move in on first start, unless the store already has that key.
- The app reads the display pick itself at startup (to open on the right
  monitor before providers start), through the same store and key.

**Consequences.** `PomodoroStorage` and Settings' load/save hooks are
gone. `tests/store_tests.cpp` covers scoping, bad keys, the side file and
the move-in. The island's position (`position.txt`) is the app's own, not a
provider's, and stays where it was.

**Open.** No size cap, and no clean-up when a provider is uninstalled
(there's no uninstalling yet). Saves happen at once on every change: fine
for a few small files; batching would come with a chatty provider.
