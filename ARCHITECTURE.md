# Architecture

How the code is organised and why. For *what* the app does, see
[docs/technical-requirements.md](docs/technical-requirements.md) (TR) and
[docs/provider-platform-spec.md](docs/provider-platform-spec.md) (PS).

## The one idea

**A pure core, and a thin Windows shell around it.**

```
 window messages,           ┌──────────── core/ (pure C++) ─────────────┐
 providers, timers  ──────► │ Event ─► reduce() ─► IslandState          │
                            │                       │                   │
                            │                       ▼                   │
                            │                    layout() ─► Scene ─────┼──► platform/ renderer
                            │                    nextDeadline() ────────┼──► one-shot timer
                            └───────────────────────────────────────────┘
```

- **`core/` decides.** It holds every rule: which mode the island is in, which
  alert shows, what's ranked first, what size the pill should be. It has no
  Windows headers, no I/O, no threads and no clock reads (time is passed in).
  That means it builds and tests on any OS.
- **`platform/` executes.** It turns window messages into core `Event`s and
  `Scene`s into Composition properties. It never decides *what* to show.
- **`app/` wires.** `main.cpp` owns the state and runs the loop:
  `event → reduce → layout → renderer`.

So "why did X happen?" is always answered in `core/`, by a scenario test.

## Layout

```
src/core/       pure logic: island.h (state machine), layout.h, tokens.h, easing.h
src/platform/   Win32 + C++/WinRT + Composition: render layer, interaction layer, renderer
src/providers/  (future) data sources: media, calendar, file shelf, …
src/app/        main.cpp: wiring only
tests/          unit tests + scenarios/*.scenario (state rules in plain text)
docs/           specs (TR, PS) and decisions/ (why things are the way they are)
```

Dependency direction is one-way: `app → platform → core`, `providers → core`.
Nothing includes "upward". `core/` never includes `platform/`.

## Data flow, concretely

1. Something happens: a click, a timer, a provider update. The platform turns
   it into a core `Event` (`PillClicked`, `AlertPosted`, `Tick`, …).
2. `reduce(state, event, now)` returns the new `IslandState`.
3. `layout(state)` returns a `Scene`: target size, offset and content per mode.
4. The renderer sets those targets. **Implicit Composition animations** do the
   actual morph (TR §5), and the interaction layer follows it (decision 001).
5. `nextDeadline(state)` says when the next timed change happens (an alert
   expires, Expanded times out). The app arms **one one-shot timer** for it.
   With no deadline nothing runs, so an idle island costs zero (TR §8).

## Rules that keep it readable

- **Plain C++20:** `struct` + free functions, `enum class`, `std::variant`,
  `std::optional`. Use a class only when it owns platform handles
  (`PillRenderer`). The only planned interface is `IProvider`.
- **No template tricks, no macros, no custom smart pointers.**
- **`winrt::` / Win32 types stay in `platform/` and `app/`.** Providers
  translate at the edge.
- **One concept per file, ~300 lines max.** Headers are the table of contents:
  short, with a one-line comment per declaration.
- **Comments say *why*, briefly**, and point at specs (`// TR §9.1`) or
  decisions (`docs/decisions/001-…`). Long reasoning goes in a decision record.
- **All tuning lives in `core/tokens.h`.** Sizes, timings, easing curves, limits.
- **Threads:** everything runs on the UI thread. The only exception is the
  compositor-tick thread, which just posts a message. Future providers post
  events to the UI thread; core has no locks.

## Testing

- **State rules → `tests/scenarios/*.scenario`.** One line per step, with the
  expected mode after it. Readable as a spec and runnable anywhere. The
  format is documented at the top of `tests/scenario_runner.cpp`.
- **Pure helpers → `tests/unit_tests.cpp`.** A tiny `expect()`, no framework.
- **Platform code** is verified by running the app (no automated tests yet).

## Status

| Area | State |
| --- | --- |
| State machine: all PS §3 modes, alert queue/pre-emption/coalescing, hidden rules | done, tested |
| Renderer: pill morph per mode, press feedback, expanded placeholder detail | done |
| Renderer: Split bubble, text/content slots (PS §5), alert pulse, receptive drop zones | not yet |
| Real input for drag (OLE drop target) and fullscreen detection (TR §9.2) | not yet; demo keys simulate them |
| Providers (`IProvider`, media, …) and plugin pipe (PS §7–8) | not yet |
| Rate limits, dismissal policies, stale activities (PS §4, §6.3) | not yet |
