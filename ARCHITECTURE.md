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
                            │                    hitTest(Scene, click) ─┼──► provider.onAction
                            │                    nextDeadline() ────────┼──► one-shot timer
                            └───────────────────────────────────────────┘
```

- **`core/` decides.** It holds every rule: which mode the island is in, which
  alert shows, what's ranked first, what size the pill should be. It has no
  Windows headers, no I/O, no threads and no clock reads (time is passed in).
  That means it builds and tests on any OS. Its one piece of global state is
  the active `tokens()`, which only the tokens.json reload writes.
- **`platform/` executes.** It turns window messages into core `Event`s and
  `Scene`s into Composition properties. It never decides *what* to show.
- **`app/` wires.** `main.cpp` owns the state and runs the loop:
  `event → reduce → layout → renderer`.

So "why did X happen?" is always answered in `core/`, by a scenario test.

## Layout

```
src/core/       pure logic: island.h (state machine), content.h (what providers show), layout.h,
                hit_test.h, provider.h (Provider/Host API), tokens.h (+ tokens_json.h), easing.h
src/platform/   Win32 + C++/WinRT + Composition: render/interaction layers, pill_renderer,
                element_visuals (one visual per content item), graphics_device (D2D/DirectWrite)
src/providers/  data sources + provider_registry. demo/ (fake player), media/ (now-playing content,
                MediaPresenter rules, SMTC provider for real Windows media)
src/app/        main.cpp: wiring only; live_tokens.cpp: tokens.json load/reload
assets/         tokens.json: the live-tunable sizes, timings and curves
tests/          unit tests + scenarios/*.scenario (state rules in plain text)
docs/           specs (TR, PS) and decisions/ (why things are the way they are)
```

Dependency direction is one-way: `app → platform → core`, `providers → core`.
Pure provider logic builds on any OS (`cup_providers` library). WinRT-backed
parts (`providers/media/smtc_*`) are Windows-only files next to it, compiled
into the app.
Nothing includes "upward". `core/` never includes `platform/`.

## Data flow, concretely

1. Something happens: a click, a timer, a provider update. The platform turns
   it into a core `Event` (`PillClicked`, `AlertPosted`, `Tick`, …).
2. `reduce(state, event, now)` returns the new `IslandState`.
3. `layout(state, now)` returns a `Scene`: target size and offset, plus every
   content item with its frame (time labels already formatted for `now`).
4. The renderer sets those targets. **Implicit Composition animations** do the
   actual morph (TR §5), and the interaction layer follows it (decision 001).
5. `nextDeadline(state)` and `sceneDeadline(scene)` say when the next timed
   change happens (an alert or activity expires, Expanded times out, a visible
   time label's second ticks over). The app arms **one one-shot timer** for the
   earliest. With no deadline nothing runs, so an idle island costs zero (TR §8).
6. A click is first checked with `hitTest(scene, point)`. A button or seekable
   progress bar becomes an `Action` routed to its provider; anything else is a
   plain `PillClicked`.

## Providers

A provider (`core/provider.h`) publishes activities and alerts through a
`Host`, and receives `Action`s for its buttons. It never touches the window or
the renderer. `ProviderRegistry` gives each provider its own Host, which
stamps the provider id on everything it posts, and routes actions back.
Content is declarative (`core/content.h`, decision 004): slots of rows of
items (text, image, icon, button, progress, time label, waveform). Layout and
drawing are the host's job, so every provider looks native.

Writing one: implement `Provider`, build `Content` in a pure function (see
`providers/media/now_playing.cpp`), test it through `ProviderRegistry` like
`tests/provider_tests.cpp`, and register it in `app/main.cpp`. WinRT event
handlers run on background threads; marshal to the UI thread before calling
the Host. `providers/media/smtc_media_provider.cpp` is the reference
example: thin WinRT reading, with every decision in a pure presenter (decision 005).

## Rules that keep it readable

- **Plain C++20:** `struct` + free functions, `enum class`, `std::variant`,
  `std::optional`. Use a class only when it owns platform handles
  (`PillRenderer`). The only interfaces are `Provider` and `Host`.
- **No template tricks, no macros, no custom smart pointers.**
- **`winrt::` / Win32 types stay in `platform/` and `app/`.** Providers
  translate at the edge.
- **One concept per file, ~300 lines max.** Headers are the table of contents:
  short, with a one-line comment per declaration.
- **Comments say *why*, briefly**, and point at specs (`// TR §9.1`) or
  decisions (`docs/decisions/001-…`). Long reasoning goes in a decision record.
- **All tuning lives in `assets/tokens.json`.** Sizes, timings, easing curves
  and limits. Edit it while the app runs and saving applies it live (see
  "Tuning" below). `core/tokens.h` declares the same fields with fallback defaults.
- **Threads:** everything runs on the UI thread. The only exception is the
  compositor-tick thread, which just posts a message. Future providers post
  events to the UI thread; core has no locks.

## Tuning (live)

1. Run the app (`.\build.ps1 -Run`) and open `assets/tokens.json` in any editor.
2. Change a value and save. The island picks it up immediately, and the
   console prints `tokens.json reloaded`.
3. If the file has a mistake (typo'd name, wrong shape, negative duration,
   syntax error), the console lists each problem with its line number and the
   previous values stay active. Nothing is half-applied.

Durations are in milliseconds and named `…Ms`. Sizes are `[width, height]`.
Curves are `[x1, y1, x2, y2]`, the same as CSS `cubic-bezier()`, so you can
paste from cubic-bezier.com. `canvasSize` only applies on restart. Watching
is event-driven (`FindFirstChangeNotification`), so it costs nothing between saves.

Adding a token: add the field to `Tokens` in `core/tokens.h`, add one line to
the field table in `core/tokens_json.cpp`, then add it to `assets/tokens.json`
(the tests fail if the file has unknown names).

## Testing

- **State rules → `tests/scenarios/*.scenario`.** One line per step, with the
  expected mode after it. Readable as a spec and runnable anywhere. The
  format is documented at the top of `tests/scenario_runner.cpp`.
- **Pure helpers → `tests/unit_tests.cpp`, `content_tests.cpp` (layout, hit-testing),
  `tokens_tests.cpp`, `provider_tests.cpp` (registry + demo provider through the real loop).**
  A tiny `expect()`, no framework.
- **Platform code** is verified by running the app (no automated tests yet).

## Status

| Area | State |
| --- | --- |
| State machine: all PS §3 modes, alert queue/pre-emption/coalescing, hidden rules | done, tested |
| Renderer: pill morph per mode, press feedback, content items (text, swimming text, art, icons, buttons, progress, waveform) | done |
| Content hover (buttons, progress knob), drag-to-seek, nested radii (decisions 002, 006) | done; platform side untested on a real build yet |
| Renderer: Split bubble, alert pulse, receptive drop zones, DPI scaling, matched geometry | not yet |
| Fullscreen detection (TR §9.2): borderless + exclusive, per monitor, maximized windows excluded | done; rules tested (`core/fullscreen`), hooks need Windows |
| Real input for drag (OLE drop target) | not yet; the D key simulates it |
| Provider API (PS §7 Tier 1), registry, demo music player | done, tested |
| Real media provider (SMTC, TR §6): art, controls, seek, multi-app switch, paused linger | done; rules tested, WinRT part needs Windows |
| Plugin pipe (PS §8 Tier 2) | not yet |
| Activity expiry (`expiresAt`) | done, tested |
| Rate limits, stale activities (PS §4, §6.3) | not yet |
| Live-tunable tokens (`assets/tokens.json`, reload on save) | done; parser tested |
