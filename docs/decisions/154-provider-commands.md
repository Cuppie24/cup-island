# 154: Providers' commands, run from Search and from Windows search

Status: Proposed. Nothing built yet.

**Context.** Providers can do things that are useful from anywhere, without
opening their card first: switch the Windows theme, open Pomodoro's window,
pause or stop the timer. People want to type them: into the island's search
(decision 079) and into Windows' own search (the Start menu). Three things
stand in the way:
- **Providers are independent** (TR §3, PS §9: no provider reads another's
  activities). If Pomodoro handed its commands to Search, or Search knew
  about Pomodoro, each would depend on the other, and switching Search off
  would take every command with it.
- **A window opens only right after the user acts on its provider**
  (decision 104, `windowOpenGrace`). A click on a Search result counts as
  acting on Search, so "Open Pomodoro window" would get `WindowOpen::NotNow`.
- **A question is drawn over its provider's own card or window** (decisions
  020, 122), never over another's. "Stop Pomodoro" asks first (decision
  126); run from Search, Pomodoro has nowhere to ask.

**Decision.**
- **Commands are the host's, like panels** (decision 009). A provider
  publishes its list, `Host::publishCommands(std::vector<CommandSpec>)`:
  each with an id, a title ("Pause the timer"), keywords, an icon, and
  whether it may go into the Start menu (`exportable`). Each call replaces
  the provider's previous list, so it lists only what applies now ("Pause"
  while running, "Resume" while paused). The registry keeps the lists,
  stamped with the provider's id; at most `commandsPerProvider` (tokens).
  Ids: letters, digits, `-`, `_`, `.`, as windows' (decision 104).
- **Running one is an `Action`.** The registry sends the owner
  `Action{providerId, actionId = command id}`, the same action its button
  sends, so a provider needs no second code path and never learns who ran it.
- **Search reads them through the host, by grant**, as it reads files
  (decision 078): `ProviderRegistry::grant("search", "commands")` allows
  `Host::findCommands(query)` and `Host::runCommand(ref)`. Without the
  grant both refuse. Search shows them as rows among its results; it never
  reaches Pomodoro, and Pomodoro never reaches it. Matching and ranking are
  pure, in `core/commands.h`, tested on Linux. Plugins (PS §8, Tier 2)
  publish the same way: no back doors (PS §1, rule 3).
- **A command passes the user's gesture to its owner.** Running one counts
  as the user acting on the owning provider: its `windowOpenGrace` starts,
  as a click on its card would start it, so "Open Pomodoro window" opens.
  What keeps decision 104's guard:
  - `runCommand` works only while the caller is in its own grace, i.e.
    right after the user acted on it. Search can run a command after a
    click or Enter, never from a wake-up, a timer or a service reply.
  - A provider can't run its own commands through it (it needs no host for that).
  - One gesture, one command: running it uses up the caller's grace, so a
    command can't start a chain of windows.
- **A command that asks asks in its own provider's place.** It opens its
  window (the gesture allows it) and asks there with `Dialog::windowId`
  (decision 122), as Pomodoro's stop does in its window (decision 126). A
  provider without a window marks the command `showsCard`: the host shows
  that provider's card (its panel's) first, then routes the action, so
  `Host::ask` lands over it. No question is ever drawn over Search's card.
  Commands don't skip the question for an Undo toast: stopping loses a
  session's progress, and an undo for every such command is more work than
  asking.
- **Windows search, through Start menu shortcuts.** Windows search finds
  programs by the shortcuts in the Start menu folder, on Windows 10 1809 and
  11 alike (decision 015); third-party apps can't add results to it any other
  way. Jump lists show only on a right-click of the app's icon, and
  PowerToys Run or Command Palette only where PowerToys is installed.
  - Behind a setting, **off by default**: it adds a "Cup Island" folder to
    Start's app list. On, the app writes a shortcut per `exportable`
    command into `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Cup
    Island\`, named "Pomodoro - Pause or resume", each running
    `CupIsland.exe --command <provider>/<command>`; off, it deletes the
    folder. Which commands and what each file is called is pure (core);
    writing the files is the app's.
  - Windows takes a while to notice a changed shortcut, so exported
    commands are the provider's stable ones ("Pause or resume Pomodoro"),
    not ones that come and go. One that doesn't apply now is ignored or
    answered with a toast.
  - The shortcut starts a second copy, which hands the command to the
    running island as decision 021's second launch does: it lets the island
    take the foreground (`AllowSetForegroundWindow`), then sends the command
    (`WM_COPYDATA`) instead of `CupIsland.ShowYourself`, and exits. The
    island runs it as a user's gesture, as above. Not running: the copy
    starts the island, which runs the command once its providers have started.

**Consequences.**
- Search is one place that shows commands, not the one they depend on: off,
  they still run from the Start menu. Rebindable hotkeys for commands
  (decision 032's table) would be one more such place, later.
- New Host calls (`publishCommands`, `findCommands`, `runCommand`), a grant,
  and the hand-over of the gesture, in `ProviderRegistry`. Rules in
  `tests/scenarios/` and `tests/provider_tests.cpp`: a command run right
  after a click opens its window; one run from a wake-up is refused; one
  without the grant is refused; a `showsCard` command's question lands over
  its own card.
- Built in steps, a branch each: the host side (this), Search's rows, then
  the Start menu export.

**Open.** Whether a provider may export a command whose title changes
(rewriting the shortcut each time). Commands' own icons in the Start menu
(the shortcut has the app's for now).
