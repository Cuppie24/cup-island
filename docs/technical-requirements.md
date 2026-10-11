# Dynamic Island for Windows — Technical Requirements

2026-09-21 · @Someone · Referenced in code as **TR §n**

## 1. Overview

This app brings an iPhone-style Dynamic Island to Windows 11/10 as a small, always-on-top pill overlay anchored near the top of the primary display. It surfaces live status — media playback, clipboard, battery, downloads — without the user opening any other app, and expands/contracts with fluid, GPU-driven animation rather than a static widget.

Unlike Windhawk-based clones, which inject into `explorer.exe` and depend on Windows shell internals that break across OS updates, this is a **standalone WinUI 3 desktop app**. It reads system state entirely through public, documented Windows Runtime (WinRT) APIs, avoiding process injection, elevated privileges, and the stability/antivirus risks that come with them.

## 2. Goals and Non-Goals

**In scope for v1:**

- Live media session display (track, artist, art, playback controls) via SMTC
- Clipboard-change indicator (text/image copy confirmation)
- Battery status and charging-state indicator
- Smooth expand/contract/morph animation, GPU-composited
- Multi-monitor and per-monitor DPI awareness
- Light/dark theme following the system setting

**Explicitly out of scope for v1:**

- Injection into `explorer.exe` or any shell process
- Custom notification center replacing Windows' native one
- Third-party plugin/extension system for other developers
- Cloud sync, telemetry, or account requirements
- macOS/Linux support (Windows-only for this version)

## 3. Architecture Overview

Standalone single-process app, no injection. Four layers:

| Layer | Responsibility |
| --- | --- |
| Presentation | WinUI 3 window (borderless, layered, click-through where empty) hosting Composition visuals for the pill shape and content |
| Animation | `Windows.UI.Composition` compositor — implicit/expression animations for expand, contract, and morph transitions |
| Data providers | Independent, event-driven modules per data source (media, clipboard, battery, downloads), each publishing state changes to the presentation layer |
| Platform integration | Win32 interop for window placement, always-on-top, multi-monitor/DPI, and startup registration |

Data providers run as async listeners (event-driven, not polling loops) and push updates through a lightweight in-process event bus to the UI layer, which triggers the relevant Composition animation.

## 4. Technology Stack

| Component | Choice | Notes |
| --- | --- | --- |
| Language | C++/WinRT | Native, header-only WinRT projection, no CLR/GC overhead |
| UI framework | WinUI 3 (Windows App SDK) | Latest stable Windows App SDK release at build time |
| Animation engine | `Windows.UI.Composition` (Composition API) | Backed by DirectComposition; implicit + expression animations |
| Rendering | Direct2D (via Composition's `SpriteVisual`/`CompositionDrawingSurface`) | For custom pill shape, blur, and rounded-rect drawing beyond standard XAML brushes |
| Media data | `GlobalSystemMediaTransportControlsSessionManager` (`Windows.Media.Control`) | Public WinRT API, no injection required |
| Build system | MSBuild / Visual Studio 2022+ | Windows App SDK project templates |
| Packaging | MSIX | Standard WinUI 3 distribution and update path |
| Minimum OS | Windows 10 1809 (build 17763) and Windows 11, both fully supported | Floor set by the media session API (SMTC); newer APIs only behind runtime checks with fallbacks (decision 015) |

## 5. Rendering and Animation Requirements

- All shape, size, and position transitions (collapsed pill ↔ expanded pill ↔ per-content layouts) MUST run as Composition **implicit animations** on `Offset`, `Size`, `Scale`, and `Clip` properties — never manual per-frame redraws on the UI thread.
- Target frame rate: match the display's native refresh rate (60/120/144 Hz) via DirectComposition's vsync-synced scheduling; no fixed-timestep `SetTimer`/`DispatcherTimer` loops driving visual state.
- Morph animation (collapsed → expanded) budget: 250–350 ms, cubic-bezier ease-out, matching the perceived speed of iOS's Dynamic Island.
- Blur/vibrancy background: implemented via Composition's built-in backdrop brush (`Windows.UI.Composition.SystemBackdrops` or `CompositionBackdropBrush`), not a custom Gaussian blur pass, to keep GPU cost low.
- Idle state (nothing to show) renders zero or near-zero GPU/CPU load — no continuous redraw loop while static.
- All animation triggers are **event-driven** (a data provider pushes a state change) rather than polled on a timer, to avoid unnecessary composition work.

## 6. Media Integration Requirements

- Use `GlobalSystemMediaTransportControlsSessionManager.RequestAsync()` to obtain the session manager; no injection or reverse-engineered Spotify APIs.
- Support **all** SMTC-registered sources (Spotify, browsers, VLC, Apple Music, etc.), not a Spotify-specific integration.
- Subscribe to `MediaPropertiesChanged`, `PlaybackInfoChanged`, and `TimelinePropertiesChanged` — updates MUST be event-driven, never polled.
- Display: track title, artist, album art (`TryGetMediaPropertiesAsync`), and play/pause state.
- Controls: play/pause, next, previous via `TryPlayAsync`/`TryPauseAsync`/`TrySkipNextAsync`/`TrySkipPreviousAsync`.
- When multiple sessions exist (e.g., Spotify + a browser tab), default to `GetCurrentSession()`; allow manual override if more than one session reports "Playing" simultaneously.

## 7. Additional Data Sources

| Feature | API | Notes |
| --- | --- | --- |
| Clipboard | `Windows.ApplicationModel.DataTransfer.Clipboard.ContentChanged` event | Event-driven; show a brief confirmation pill, not full clipboard contents by default (privacy) |
| Downloads | No public system-wide API — scope to browser-specific integration (e.g., watch the Downloads folder via `FileSystemWatcher`) | Explicitly best-effort for v1; document the limitation rather than reverse-engineering browser internals |
| Notifications | Windows App SDK `AppNotificationManager` (for the app's own notifications only) | Do NOT attempt to intercept other apps' toast notifications — no public API exists without injection, which is out of scope |

## 8. Performance and Resource Requirements

- Idle CPU usage: under 0.5% on a mid-range 4-core CPU when no active state changes are occurring.
- Idle memory footprint: under 60 MB working set.
- GPU usage during animation: composited transitions only; no measurable GPU load while the pill is static.
- No timer-based polling loops for any data source — all providers listed in sections 6–7 are event-driven.
- Respect system power state: reduce or suspend animation smoothing when the system reports Battery Saver (`EnergySaverStatus.On`); never block or interfere with Modern Standby / Connected Standby.
- App must not prevent `explorer.exe` restarts, sleep, or shutdown; no held handles or locks on shell resources.

## 9. Window and Shell Integration Requirements

- Window style: borderless, topmost, layered (`WS_EX_LAYERED` + `WS_EX_TOOLWINDOW`), click-through on transparent regions so it never intercepts input meant for underlying apps.
- Not present in the taskbar or Alt-Tab switcher.
- Correct placement and scaling across multiple monitors with independent DPI settings; recalculate position on display topology change (`DisplayInformation` events).
- Follows system light/dark theme automatically (`UISettings.ColorValuesChanged`).
- Launch at sign-in via the standard MSIX startup task mechanism (`StartupTask` API) — user opt-in required, no silent registry autorun.

### 9.1 Two-Layer Window Architecture

The overlay MUST be split into two separate windows rather than one:

- **Render layer** — a single large, `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST` window sized to the pill's maximum expanded footprint. `WS_EX_TRANSPARENT` excludes it from hit-testing entirely, so it never intercepts input anywhere on its surface. All Composition visuals, animations, and content rendering live here.
- **Interaction layer** — a small, non-transparent topmost window, resized/repositioned to track only the pill's *currently visible* bounds, so it can receive clicks (media controls, expand/collapse) without ever covering more of the screen than the visible island.

This is the same pattern used by Discord's overlay, Steam overlay, Xbox Game Bar, and RTSS — it lets the app draw over the full screen while remaining click-through except in explicitly interactive regions, without falling back to injection or manual per-pixel hit-region hacks.

The interaction layer's size/position changes are NOT themselves animated — `SetWindowPos` snaps instantly, with nothing rendered there to see. However, its bounds MUST be sampled from the render layer's *current* animated Composition values every composition frame (not driven by an independent timer), or the clickable region drifts out of sync with the visible pill — causing missed clicks during the animation or a hit target that extends past the visible shape. This per-frame `SetWindowPos` on a small, invisible window is cheap and does not reintroduce the resize cost ruled out for the render layer.

### 9.2 Fullscreen and Borderless Detection

The island MUST hide automatically over exclusive-fullscreen and borderless-fullscreen apps, and MUST NOT hide over normal maximized windows (e.g., a maximized File Explorer).

Detection logic:

1. On foreground-window change (`EVENT_SYSTEM_FOREGROUND` via `SetWinEventHook`) and on window move/resize, get the foreground window's bounds (`GetWindowRect`) and its monitor's `rcMonitor`/`rcWork` (`MonitorFromWindow` + `GetMonitorInfo`).
2. If the window's bounds match the monitor's full `rcMonitor` (taskbar area covered), treat it as fullscreen and hide the island. If bounds match only `rcWork` (taskbar still visible), the island stays visible — this is how a maximized Explorer window is distinguished from borderless-fullscreen.
3. Cross-check with `SHQueryUserNotificationState()` for `QUNS_RUNNING_D3D_FULL_SCREEN` to catch true D3D exclusive-fullscreen apps and reduce false positives from apps that happen to cover the full monitor without being real fullscreen.
4. Re-evaluate per monitor independently, so a fullscreen app on one display doesn't hide the island on another.

Note: true DXGI exclusive-fullscreen bypasses the desktop compositor entirely, so the island cannot render above it regardless of z-order settings — detection here is about hiding gracefully, not a technical workaround to render through it.

## 10. Non-Functional Requirements

- **Stability:** runs in its own process; a crash never affects `explorer.exe` or other apps. Auto-restart on unhandled exception, with a cooldown to avoid crash loops.
- **Startup time:** cold start to first visible pill under 1.5 seconds on typical hardware (C++/WinRT gives this headroom over a managed-runtime equivalent).
- **Packaging:** MSIX package, code-signed, distributed via direct download and optionally the Microsoft Store.
- **Updates:** MSIX auto-update or an in-app update check against a versioned manifest; no forced background updates without user notice.
- **Privacy:** no network calls beyond update checks; clipboard/media data never leaves the device.
- **Accessibility:** pill content exposed via UI Automation so screen readers can announce media/battery state changes.

## 11. Development Milestones

| Phase | Deliverable |
| --- | --- |
| 1 — Prototype | Borderless layered window, static pill shape, manual expand/collapse toggle (no real animation yet) |
| 2 — Animation core | Composition API implicit/expression animations wired to expand/contract/morph states; frame-rate verification across displays |
| 3 — Media integration | SMTC session manager wired in, live track/art/controls, multi-session handling |
| 4 — Secondary data sources | Clipboard and battery providers, event-driven updates feeding the same animation pipeline |
| 5 — Shell polish | Multi-monitor/DPI correctness, theme following, startup task, click-through behavior |
| 6 — Packaging & release | MSIX packaging, code signing, crash reporting, v1 release candidate |
