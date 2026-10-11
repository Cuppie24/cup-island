# Cup Island

An iPhone-style Dynamic Island for Windows. It's a standalone app with no
shell injection, and it's designed as a platform that any data source can
publish to.

- What it does: [docs/technical-requirements.md](docs/technical-requirements.md),
  [docs/provider-platform-spec.md](docs/provider-platform-spec.md)
- How the code works: [ARCHITECTURE.md](ARCHITECTURE.md)

## Build

```powershell
.\build.ps1 -Run          # Windows: full app, Release (needs Visual Studio "Desktop development with C++")
.\build.ps1 -Debug -Run   # Windows: Debug build, for stepping through code
```
In Visual Studio: **File → Open → Folder**, pick the *Windows x64 Debug* or
*Release* configure preset in the toolbar, select `CupIsland.exe` as startup item, F5.
```sh
cmake --preset core && cmake --build --preset core && ctest --preset core   # Linux/macOS: core + tests
```

**Tuning:** while the app runs, edit [`assets/tokens.json`](assets/tokens.json)
(sizes, durations, easing curves) and save. Changes apply instantly. See
ARCHITECTURE.md → Tuning.

The island shows whatever Windows is playing (Spotify, browser tabs, VLC,
Media Player…): album art and a live waveform in the compact pill, and title,
artist, progress and ⏮ ⏯ ⏭ when you click it. Clicking the progress bar
seeks. With several players, ⇄ switches between them. Paused music stays
for 5 minutes (`mediaPausedLingerMs`).

Demo keys (once the pill has focus): **M** fake music player on/off,
**T** second activity, **C** critical alert, **D** simulated drag, **H** hide,
**Esc** collapse/quit.
