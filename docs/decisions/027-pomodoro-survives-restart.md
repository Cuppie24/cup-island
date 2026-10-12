# 027: A Pomodoro session survives a restart

Status: Accepted

**Context.** Quitting the app (an update, a crash, a reboot) threw away a
running Pomodoro session: the user came back to a dormant timer and had to
guess where they were. The countdown is a `Timeline` on the app's own
clock, which starts over every run, so it can't simply be written down.

**Decision.**
- Pomodoro saves its session on every change (start, pause, resume, a
  phase change) as one line (`pomodoro_session.h`): phase, round, playing,
  the position, the phase's duration, and the **wall clock** at that
  moment. Stopping, or the session ending, saves nothing (""). Ticks aren't
  saved: the position follows from the timeline.
- On start, a saved session resumes as it would be now:
  - a paused one is exactly where it was;
  - a running one has gone on while the app was closed. Phases that ended
    meanwhile pass quietly (no alerts for the past), and a session whose
    long break ended meanwhile is gone, leaving the panel dormant.
  - A clock set back counts as no time passed. Unreadable text is ignored.
- The phase sequence (`followingPhase`) is shared by the live timer and the
  catch-up, so they can't disagree.
- Storage and the wall clock are handed in by the app (`PomodoroStorage`,
  `pomodoro.txt` next to `todos.txt`), like the to-do list, so the provider
  stays pure and the catch-up is tested on Linux.

**Consequences.** `tests/pomodoro_tests.cpp` runs two app lifetimes with a
fake wall clock. The current phase keeps its saved duration; phases after it
use tokens.json's at the time.

**Open.** No "while you were away" summary. If more providers need to save
things, a shared store on `Host` could replace each one's hand-in.
