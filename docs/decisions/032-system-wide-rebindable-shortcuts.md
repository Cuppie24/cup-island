# 032: System-wide shortcuts, rebindable, refused on conflict

Status: Accepted (supersedes 031's shortcuts)

**Context.** Decision 031's Ctrl+X only worked while the island had
keyboard focus, which is rarely: you click somewhere else and it's gone.
People want to reach Home from any app, with keys of their choosing.

**Decision.**
- **Commands are system-wide hotkeys** (`RegisterHotKey`, Windows 2000+,
  so fine for decision 015), registered on the interaction window, one id
  per command (`platform/hotkeys`). `MOD_NOREPEAT`: held down, one press.
- **Open Home is Ctrl+Q** by default. That's the user's pick, knowing the
  cost: registered system-wide, Ctrl+Q no longer reaches any other app while
  the island runs (it's Quit in some). Ctrl+Alt+Space was the first default,
  then Ctrl+X, dropped because losing Cut everywhere hurt more; a Ctrl+X
  already saved in Settings stays until changed there. Pressed again while Home is open, it
  closes it.
- **Off while the island is hidden** (a borderless or exclusive fullscreen
  app, TR §9.2, or any other hiding): the shortcuts are unregistered, so a
  game or a fullscreen video player gets its keys, and registered again when
  the island shows (`SettingsProvider::setHotkeysActive`, from the app on
  every change of `IslandState::hidden`). Started under a fullscreen app,
  none are registered until it's gone. The island comes to the front with it (Windows lets the app
  that gets a hotkey do that), so the arrows work straight away.
- **Every command's keys can be changed** on Settings' Keyboard shortcuts
  page: a click on its row says "Press keys…", and the next key pressed on
  the island (with its modifiers) is the new chord. Esc leaves it. The
  arrows, Enter and Esc that move around the island are fixed and listed
  under "On the island": they only act while the island has focus, so
  they never clash with another app.
- **Any conflict aborts the change**, the old keys stay, and the page says
  why (`bindingProblem`, pure):
  - no Ctrl, Alt or Win ("A shortcut needs Ctrl, Alt or Win"): a bare key
    or Shift+key, system-wide, would stop that key typing anywhere;
  - another command's keys ("Ctrl+Alt+H is already Open Home");
  - another app has them: Windows refuses the registration, and the old
    keys are registered again ("… is taken by another app").
  Pressing a command's current keys while recording arrives as that hotkey,
  not a key press; it's handled the same as pressing them (own keys: no
  change; another command's: the conflict note).
- **Saved** in the Settings provider's store (`shortcuts`), a line per
  command, `home=Ctrl+Q`, the same text Settings shows. A line
  that can't be read, or bound, keeps the default. One that another app
  has at startup stays as saved, and the page says it's taken.
- Alt combinations now reach the app as `WM_SYSKEYDOWN` too, only for
  recording; Windows' own handling (Alt+F4) still runs.

**Consequences.** Chords, the saved form and the conflict rules in
`core/shortcuts`, tested in `tests/keyboard_tests.cpp`; recording,
aborting and going off while hidden in `tests/shortcut_settings_tests.cpp`.
The app's side (focus, Enter, recording, WM_HOTKEY) is `app/keyboard_input`. Registration and the hotkey arriving
need a Windows run. Decision 050 added a second command (Open the
activity), so two commands clashing is tested too.
