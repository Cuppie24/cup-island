# Cup Island

A Dynamic Island for Windows: a standalone C++/WinRT app (Win32 + Composition).

## Read first
- `ARCHITECTURE.md`: layout, data flow, coding rules. Short; read it before changing code.
- `src/core/island.h`: the state machine API, the heart of the app.
- `src/core/provider.h` + `content.h`: how data sources put things on the island (ARCHITECTURE.md → Providers).
- Specs: `docs/technical-requirements.md` (cited as **TR §n**), `docs/provider-platform-spec.md` (**PS §n**).
  Read only the section you need.
- `docs/decisions/`: why non-obvious things are the way they are. Check here before "fixing" something odd.

## Build and test
- Core + tests (Linux/macOS; use this in Linux sessions):
  `cmake --preset core && cmake --build --preset core && ctest --preset core`
- Full app (Windows only): `.\build.ps1 -Run` (Release) or `-Debug -Run`; presets `windows` / `windows-debug` (Ninja + MSVC). `platform/` and `app/` can't be built on Linux.

## Rules
- Logic goes in `src/core/` (pure: no Windows headers, no I/O, no clock reads). Platform code only translates.
- Provider logic that doesn't need WinRT (content building, decisions) lives in `src/providers/` and is tested on Linux.
- Every state rule change gets a line in `tests/scenarios/*.scenario`. Run the tests before committing.
- Sizes, timings and limits: fields in `src/core/tokens.h`, values in `assets/tokens.json` (live-reloaded). Never hard-code them.
- Plain C++20 subset: structs + free functions, enum class, variant/optional. No macros, no template metaprogramming.
- Files ~300 lines max; headers are short and commented. Comments say why, and cite TR/PS/decisions.
- Don't read `build/`.
