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
                lift.h + placement.h (moving the island along the top edge, decision 026),
                keyboard.h (arrow, Tab and digit focus, decisions 031, 033) + shortcuts.h (the shortcut table, decision 031),
                tokens.h (+ tokens_json.h, json_reader.h), easing.h
src/platform/   Win32 + C++/WinRT + Composition: render (monitor-wide)/interaction layers, pill_renderer (two blobs:
                pill + Split bubble, and the liquid_bridge between them), element_visuals (one visual
                per content item), graphics_device (D2D/DirectWrite), tray_icon (right-click → Exit), hotkeys (system-wide shortcuts), autostart (Start with Windows, set from Settings), log (the log file), watchdog (hang/crash dumps), stay_on_top
src/providers/  data sources + provider_registry. demo/ (fake player), media/ (now-playing content,
                MediaPresenter rules and the media widget, SMTC provider for real Windows media),
                pomodoro/ (the first panel), system/ (Home's theme, Bluetooth and Settings widgets:
                app themes, display, Start with Windows, Copy logs), update/ (updates from GitHub
                Releases: versions and offers, pure; github.cpp fetches and installs, decision 036)
src/app/        main.cpp: wiring only; live_tokens.cpp: tokens.json load/reload; keyboard_input.cpp: keyboard focus,
                Enter/Space, system-wide shortcuts and recording new keys (decisions 031, 032)
assets/         tokens.json: the live-tunable sizes, timings and curves; the app icon (cup_island.rc/.ico)
installer/      Inno Setup script for the Windows installer (decision 011); its version comes from build.ps1
.github/        workflows/release.yml: the Release button (build, test, tag, publish; decision 036)
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
items (text, image, icon, button, progress, time label, waveform, swatch). Layout and
drawing are the host's job, so every provider looks native.

A provider can also publish **panels** (decision 009): always-available
entries shown as widgets on Expanded's Home, not ranked into the pill.
Widgets only inform (decision 019): no buttons; a click opens the card. Starting
an activity with a panel's id makes it live; ending it makes it dormant again.
Providers that change on a schedule ask `Host::wakeAt` and get `onWake()`.
Ctrl+W on an activity's card calls `onCloseRequested()`: the provider ends
it, asks first, or ignores it (the default; decision 034).
Keyboard navigation is the host's: providers never see keys or move
focus. Focus follows the layout's reading order, and where the host
can't tell, a provider gives a hint (`Content::backAction`), not a key
handler (decision 035).
What a provider keeps between runs goes through `Host::load/save` (decision
028): text under its own keys, in its own format; never its own files.
Lists (`Row::list`, decision 029) are scrolled, reordered and edited by the
host; the provider publishes every row and gets the result as actions.
`providers/pomodoro/` is the reference panel.

Providers decide *what* goes where (items, rows, order, alignment, which
card preset and text style); the host decides *how it looks* (every size,
font and colour, from tokens.json). See decision 014.

Writing one: implement `Provider`, build `Content` in a pure function (see
`providers/media/now_playing.cpp`), test it through `ProviderRegistry` like
`tests/provider_tests.cpp`, and register it in `app/main.cpp`. WinRT event
handlers run on background threads; marshal to the UI thread before calling
the Host. `providers/media/smtc_media_provider.cpp` is the reference
example: thin WinRT reading, with every decision in a pure presenter (decision 005).

## Rules that keep it readable

- **Windows 10 1809+ and Windows 11, both first-class (decision 015).** Any
  API newer than 1809 is looked up at runtime (by name, never ordinal) and
  has a fallback. One static import of a Windows 11 function stops the exe
  from starting on Windows 10 at all.
- **Plain C++20:** `struct` + free functions, `enum class`, `std::variant`,
  `std::optional`. Use a class only when it owns platform handles
  (`PillRenderer`). The only interfaces are `Provider` and `Host`.
- **No template tricks, no macros, no custom smart pointers.**
- **No names `windows.h` defines as macros** (`small`, `near`, `far`,
  `hyper`, `interface`, `IN`, `OUT`, `OPTIONAL`…), even in core: core is
  included after it on Windows, and Linux builds won't catch it.
  The SDK also declares some global types (`Chord`): in `app/main.cpp`,
  which has `using namespace cup`, write such core types as `cup::Chord`.
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

Durations are in milliseconds and named `…Ms`. Sizes are `[width, height]`,
in logical pixels: the app multiplies every length by the display's scale
and by `islandScale`, the one knob for the island's overall size (decision
017). At 150% with `islandScale` 0.85, a 380 card is 485 physical pixels.
Font weights (`…FontWeight`, 100..900) and `textContrast` are not scaled;
decision 024 says what each text token does and which way to turn it.
Colours are `[r, g, b]` or `[r, g, b, alpha]` (0..255, alpha 0..1, like CSS `rgba()`).
Curves are `[x1, y1, x2, y2]`, the same as CSS `cubic-bezier()`, so you can
paste from cubic-bezier.com. A new `canvasSize` (or `islandScale`) resizes the render layer. Watching
is event-driven (`FindFirstChangeNotification`), so it costs nothing between saves.

Adding a token: add the field to `Tokens` in `core/tokens.h`, add one line to
the field table in `core/tokens_json.cpp`, then add it to `assets/tokens.json`
(the tests fail if the file has unknown names).

Themes (decision 022) are the one nested object in tokens.json: `"themes"`
maps a name to the colour tokens it changes. The picked theme's colours go
over the file's, so `tokens()` is always what's on screen.

## Testing

- **State rules → `tests/scenarios/*.scenario`.** One line per step, with the
  expected mode after it. Readable as a spec and runnable anywhere. The
  format is documented at the top of `tests/scenario_runner.cpp`.
- **Pure helpers → `tests/unit_tests.cpp`, `content_tests.cpp` (layout, hit-testing),
  `home_tests.cpp` (tiles, Home tab, idle clock), `item_tests.cpp` (ring, steps, accent), `tokens_tests.cpp`,
  `provider_tests.cpp` and `pomodoro_tests.cpp` (registry + provider through the real loop),
  `media_tests.cpp` (media presenter and widget), `keyboard_tests.cpp` (arrow-key focus, shortcuts), `system_tests.cpp` (theme and Bluetooth presenters), `settings_tests.cpp` and `shortcut_settings_tests.cpp` (Settings, its shortcuts page; `panel_host.h` is their Host), `todo_tests.cpp` (the to-do list end to end), `update_tests.cpp` (versions, offers, the updater's checks and install).**
  A tiny `expect()`, no framework.
- **Platform code** is verified by running the app (no automated tests yet).

## Status

| Area | State |
| --- | --- |
| State machine: all PS §3 modes, alert queue/pre-emption/coalescing, hidden rules | done, tested |
| Renderer: pill morph per mode, press feedback, content items (text, swimming text, art, icons, buttons, progress, waveform) | done |
| Content hover (buttons, progress knob), drag-to-seek, nested radii (decisions 002, 006) | done; platform side untested on a real build yet |
| Renderer: Split bubble with liquid neck, drag-to-swap (decision 007) | done; rules tested, visuals need Windows |
| Display scaling: tokens in logical pixels, scaled per the primary display, live on a scale change (decision 017) | done; scaling tested, Windows side needs a run |
| Island shadow: a DropShadow under the pill and the bubble, masked by a live snapshot of each (decision 018) | done; needs a Windows run to judge |
| Dialogs over a card, scrolling lists, buttons shown on hover (decision 020) | done; rules tested, Windows side needs a run |
| Pomodoro session survives a restart: saved with the wall clock, caught up on start (decision 027) | done; tested |
| Providers' store: `Host::load/save/wallClock`, a file per key, legacy files moved in (decision 028) | done; tested |
| Host lists: smooth scrolling, keep-in-view, drag to reorder, edit in place (decision 029) | done; rules tested, compositor side needs a Windows run |
| Renderer: alert pulse, receptive drop zones, matched geometry | not yet |
| Fullscreen detection (TR §9.2): borderless + exclusive, per monitor, maximized windows excluded | done; rules tested (`core/fullscreen`), hooks need Windows |
| Real input for drag (OLE drop target) | not yet; the D key simulates it |
| Provider API (PS §7 Tier 1), registry, demo music player | done, tested |
| Switching activities (decision 007): bubble click, drag-to-swap, Expanded tabs, sticky pick | done; rules tested, tab visuals need Windows |
| Real media provider (SMTC, TR §6): art, controls, seek, multi-app switch, paused linger | done; rules tested, WinRT part needs Windows |
| Plugin pipe (PS §8 Tier 2) | not yet |
| Activity expiry (`expiresAt`) | done, tested |
| Rate limits, stale activities (PS §4, §6.3) | not yet |
| Live-tunable tokens (`assets/tokens.json`, reload on save) | done; parser tested |
| Panels, Home (tile grid, Home button), tabs at the bottom, idle clock, provider wake-ups (decision 009) | done; rules tested, visuals need Windows |
| Widgets: one per provider, information only, media always on Home (decision 019) | done; rules tested, visuals need Windows |
| Theme toggle and Bluetooth widgets | presenters tested; Windows readers need Windows |
| Pomodoro (first panel): focus rounds, breaks, pause/skip/stop, ring, phase dots | done, tested |
| Progress ring, Steps, accent colour (decision 010) | done; rules tested, drawing needs Windows |
| Text input (typing, paste) and Pomodoro's to-do list (decision 016) | done; rules tested, typing and drawing need Windows |
| Release: no-console exe, static C++ runtime, tray icon with Exit, Inno Setup installer (decision 011) | done; needs a Windows build to verify |
| MSIX, code signing, StartupTask (TR §9, §10) | not yet |
| Log file, Copy logs (in Settings), hang/crash minidumps, exit without deadlock (decision 013) | done; button rules tested, the rest needs Windows |
| Settings widget, app themes (dark and light) from tokens.json, saved per user (decision 022) | done; rules tested, visuals need Windows |
| Settings as a list: Theme and Display pages of choices, Start with Windows on by default, stays open on a near miss (decision 030) | done; rules tested, visuals need Windows |
| Keyboard: arrows move focus on cards and Home, Enter/Space click (decision 031) | done; navigation tested, key handling needs Windows |
| Keyboard: Tab/Shift+Tab, Home/End, Ctrl+Tab switches tabs, 1–9 open Home's widgets, Esc one step back, a focus ring (decision 033) | done; navigation and Esc tested, keys and ring need Windows |
| Ctrl+W asks the activity's provider to close it: Pomodoro confirms, media ignores (decision 034) | done; tested, the key needs Windows |
| System-wide shortcuts (Ctrl+X: Home), off while a fullscreen app hides the island, rebindable in Settings, a change aborted on any conflict (decision 032) | done; rules tested, registration needs Windows |
| Releases from a workflow, version from the tag, updates from GitHub Releases: daily check, widget + alert, SHA-256-checked silent install (decision 036) | rules tested; the workflow and the Windows side need a run |
