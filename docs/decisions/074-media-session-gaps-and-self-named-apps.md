# 074: A media session's brief gaps, and apps that name themselves

Status: Accepted

**Context.** Yandex Music (an Electron app) showed two bugs. On every track
change the open media card switched to "Not playing" and back: the app
drops its Windows media session and makes a new one, and for that moment
Windows lists no session. The log shows it as `showing
ru.yandex.desktop.music` again with no `Windows' main media` line before it.
The presenter ended the activity on the first read with no session.
Clicking the art (decision 005) did nothing (`no window found for
ru.yandex.desktop.music`): the app's session id is a name it chose, not
its exe, and Electron sets that id for the process, not on its windows, so
neither the window's id nor the exe name matched.

**Decision.**
- **A grace before "nothing playing":** with no session left, what's shown
  stays for `mediaSessionGoneGraceMs` (1.5 s) through `Host::wakeAt`; a
  session back within it carries on as if it never went. Its next track
  then alerts like any track change in the same app (the gap used to make
  it look like a new app, so it didn't). The grace starts at the first read
  with no session; more such reads don't extend it. Stopping the provider
  ends at once. Rules in `MediaPresenter`, tested.
- **The exe from the Start menu:** an app with its own id puts that id on
  its Start menu shortcut, so its notifications work. When the id isn't an
  exe name, the target of `shell:AppsFolder\<id>` (`System.Link.TargetParsingPath`)
  gives the exe to look for windows and a running process by. If nothing
  is found, the log line names the exe names tried.

**Consequences.** A player that really closes stays on the island 1.5 s
longer. The shortcut lookup needs the app to have one carrying its id;
without one it's still "no window found", as before.
