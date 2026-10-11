# Cup Island

An iPhone-style Dynamic Island for Windows. It's a standalone app with no
shell injection, and it's designed as a platform that any data source can
publish to.

- What it does: [docs/technical-requirements.md](docs/technical-requirements.md),
  [docs/provider-platform-spec.md](docs/provider-platform-spec.md)
- How the code works: [ARCHITECTURE.md](ARCHITECTURE.md)

## Build

```powershell
.\build.ps1 -Run          # Windows: full app (needs Visual Studio with C++ and CMake tools)
```
```sh
cmake --preset core && cmake --build --preset core && ctest --preset core   # any OS: core + tests
```

**Tuning:** while the app runs, edit [`assets/tokens.json`](assets/tokens.json)
(sizes, durations, easing curves) and save. Changes apply instantly. See
ARCHITECTURE.md → Tuning.

The prototype starts empty. After a moment a demo media activity appears
with an alert, then settles. Click the pill to expand it. Demo keys (once
the pill has focus): **M** media, **T** second activity, **A** alert,
**C** critical alert, **D** simulated drag, **H** hide, **Esc** collapse/quit.
