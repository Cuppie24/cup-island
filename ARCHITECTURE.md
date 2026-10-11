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
src/core/       pure logic: island.h (state machine), alert_queue.h, content.h (what providers show), layout.h
                (+ slot_layout.h), split.h, tabs.h, home.h, hit_test.h, provider.h (Provider/Host API),
                tokens.h (+ tokens_json.h), easing.h
src/platform/   Win32 + C++/WinRT + Composition: render/interaction layers, pill_renderer (two blobs:
                pill + Split bubble, and the liquid_bridge between them), element_visuals (one visual
                per content item), graphics_device (D2D/DirectWrite), tray_icon (right-click → Start with Windows, Exit), autostart, log (the log file), watchdog (hang/crash dumps), stay_on_top
src/providers/  data sources + provider_registry. demo/ (fake player), media/ (now-playing content,
                MediaPresenter rules, SMTC provider for real Windows media), pomodoro/ (the first panel),
                system/ (Home's battery, theme and Bluetooth widgets, Copy logs)
src/app/        main.cpp: wiring only; live_tokens.cpp: tokens.json load/reload
assets/         tokens.json: the live-tunable sizes, timings and curves; the app icon (cup_island.rc/.ico)
installer/      Inno Setup script for the Windows installer (decision 011)
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
3. `layout(state, now, measure)` returns a `Scene`: target size and offset, plus every
   content item with its frame (time labels already formatted for `now`).
   `measure` is the platform's text width (DirectWrite), for the few places
   that centre text of unknown width (Expanded's tabs); core has no fonts.
4. The renderer sets those targets. **Implicit Composition animations** do the
   actual morph (TR §5), and the interaction layer follows it (decision 001).
   Geometry is set at once; new content is built in a queued call right after,
   so drawing it never delays the morph's first frame (decision 012).
5. `nextDeadline(state)`, `sceneDeadline(scene)` and the registry's
   `nextWake()` say when the next timed change happens (an alert or activity
   expires, Expanded times out, a visible time label's second or the idle
   clock's minute ticks over, a provider asked to be woken). The app arms
   **one one-shot timer** for the earliest. Nothing else runs, so an idle
   island costs one wake-up a minute, for its clock (TR §8, decision 009).
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

A provider can also publish **panels** (decision 009): always-available
entries shown as tiles on Expanded's Home, not ranked into the pill. Starting
an activity with a panel's id makes it live; ending it makes it dormant again.
Providers that change on a schedule ask `Host::wakeAt` and get `onWake()`.
`providers/pomodoro/` is the reference panel.

Providers decide *what* goes where (items, rows, order, alignment, which
card preset and text style); the host decides *how it looks* (every size,
font and colour, from tokens.json). See decision 011.

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

1. Run the app (`.\build.ps1 -Debug -Run`: only Debug builds have the console) and open `assets/tokens.json` in any editor.
2. Change a value and save. The island picks it up immediately, and the
   console prints `tokens.json reloaded`.
3. If the file has a mistake (typo'd name, wrong shape, negative duration,
   syntax error), the console lists each problem with its line number and the
   previous values stay active. Nothing is half-applied.

Durations are in milliseconds and named `…Ms`. Sizes are `[width, height]`.
Colours are `[r, g, b]` or `[r, g, b, alpha]` (0..255, alpha 0..1, like CSS `rgba()`).
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
  `home_tests.cpp` (tiles, Home tab, idle clock), `item_tests.cpp` (ring, steps, accent), `tokens_tests.cpp`,
  `provider_tests.cpp` and `pomodoro_tests.cpp` (registry + provider through the real loop),
  `system_tests.cpp` (battery and Bluetooth presenters).**
  A tiny `expect()`, no framework.
- **Platform code** is verified by running the app (no automated tests yet).

## Status

| Area | State |
| --- | --- |
| State machine: all PS §3 modes, alert queue/pre-emption/coalescing, hidden rules | done, tested |
| Renderer: pill morph per mode, press feedback, content items (text, swimming text, art, icons, buttons, progress, waveform) | done |
| Content hover (buttons, progress knob), drag-to-seek, nested radii (decisions 002, 006) | done; platform side untested on a real build yet |
| Renderer: Split bubble with liquid neck, drag-to-swap (decision 007) | done; rules tested, visuals need Windows |
| Renderer: alert pulse, receptive drop zones, DPI scaling, matched geometry | not yet |
| Fullscreen detection (TR §9.2): borderless + exclusive, per monitor, maximized windows excluded | done; rules tested (`core/fullscreen`), hooks need Windows |
| Real input for drag (OLE drop target) | not yet; the D key simulates it |
| Provider API (PS §7 Tier 1), registry, demo music player | done, tested |
| Switching activities (decision 007): bubble click, drag-to-swap, Expanded tabs, sticky pick | done; rules tested, tab visuals need Windows |
| Real media provider (SMTC, TR §6): art, controls, seek, multi-app switch, paused linger | done; rules tested, WinRT part needs Windows |
| Plugin pipe (PS §8 Tier 2) | not yet |
| Activity expiry (`expiresAt`) | done, tested |
| Rate limits, stale activities (PS §4, §6.3) | not yet |
| Live-tunable tokens (`assets/tokens.json`, reload on save) | done; parser tested |
| Panels, Home (tile grid, clock tile, Home button), tabs at the bottom, idle clock, provider wake-ups (decision 009) | done; rules tested, visuals need Windows |
| Battery, theme toggle and Bluetooth widgets | presenters tested; Windows readers need Windows |
| Pomodoro (first panel): focus rounds, breaks, pause/skip/stop, ring, phase dots | done, tested |
| Progress ring, Steps, accent colour (decision 010) | done; rules tested, drawing needs Windows |
| Release: no-console exe, static C++ runtime, tray icon with Start with Windows + Exit, Inno Setup installer (decision 011) | done; needs a Windows build to verify |
| MSIX, code signing, StartupTask (TR §9, §10) | not yet |
| Log file, Copy logs tile, hang/crash minidumps, exit without deadlock (decision 013) | done; tile rules tested, the rest needs Windows |
