# 015: Windows 10 is a supported platform, not best-effort

Status: Accepted

**Context.** The installer's build wouldn't start on a Windows 10 PC: "ordinal
1104 not found". One Windows 11-only function
(`DCompositionWaitForCompositorClock`) was linked statically, and Windows
refuses to load an exe whose imports it can't resolve. Nothing of ours ran,
not even logging, so there was nothing to diagnose from inside the app.
Development happens on Windows 11, where nothing looks wrong.

**Decision.** Cup Island runs on **Windows 10 version 1809 (build 17763) and
later, and on Windows 11**, both fully supported.

Before using a Windows API, check "Minimum supported client" on its docs
page. Anything newer than 1809 is never linked or called unconditionally:
- **Win32:** look it up at runtime with `GetProcAddress` **by name**, never
  by ordinal. Ordinals differ between Windows builds and may belong to a
  different function.
- **WinRT:** check with `ApiInformation`, or call it inside try/catch.
- **Windows 11-only looks** (Mica and other backdrops, DWM corner
  preferences): optional extras, switched on at runtime only.

Every such use needs a fallback that keeps the feature working, or that
leaves it out cleanly.

**What sets the floor today:**

| API | Needs |
| --- | --- |
| Media sessions (`Windows.Media.Control`, SMTC) | 1809, the highest |
| Composition shapes, gradient brushes, `DesktopWindowTarget` | 1803 |
| `DispatcherQueue` (`CreateDispatcherQueueController`) | 1709 |
| Per-monitor DPI awareness v2 | 1703 |
| PowerManager, device enumeration, `DWMWA_CLOAKED`, `SHQueryUserNotificationState` | older |

**Newer APIs we use, with their fallbacks:**

| Newer than 1809 | Fallback | Where |
| --- | --- | --- |
| `DCompositionWaitForCompositorClock` (Windows 11) | `DwmFlush()` | `platform/interaction_layer.cpp`, decision 001 |
| `AppInfo::GetFromAppUserModelId` (2004) | app name from the exe name | `providers/media/smtc_reading.cpp` |
| Segoe Fluent Icons, Segoe UI Variable (Windows 11 fonts) | Segoe MDL2 Assets, Segoe UI | `platform/element_painting.cpp` |
| Font weight axis (`IDWriteTextFormat3`, 1803) and grayscale contrast (`IDWriteFactory1`), queried with `try_as` | the named weight; plain enhanced contrast | `platform/element_painting.cpp`, `graphics_device.cpp`, decision 024 |

**Checking.**
- On the Release exe, in Developer PowerShell:
  - `dumpbin /dependents build\windows\CupIsland.exe` should list no new DLLs.
  - `dumpbin /imports` should show no "Ordinal" imports from DLLs whose
    newer exports are ordinal-only, such as `dcomp.dll`.
- Before a release, install it on a Windows 10 machine or VM.
- The log line `frame clock: …` says which path ran.

**Consequences.** A little more code at every new-API call site, and a
fallback to design each time. Raising the floor above 1809 is a decision of
its own: write a new decision record that supersedes this one.
