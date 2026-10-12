# 023: Choosing the display the island is on

Status: Accepted; the arrows superseded by 030 (the same page of choices as Theme)

**Context.** The island always sat at the top of the primary display: the
render layer was centred with `SM_CXSCREEN` at y = 0, and its scale came
from `primaryDisplayScale()`. People with several monitors want it on a
particular one. Decision 017 also left "a resolution change doesn't
re-centre it" open.

**Decision.**
- **A Display row on the Settings card** (decision 022), with the same
  ‹ name › arrows as Theme. It's shown only with two or more monitors.
  The island moves at once, so the island is the preview.
- **Choices:** "Main display" first. It's the default, and it follows
  Windows' primary display, also when the user changes which one is
  primary. Then each monitor by its friendly name ("DELL U2720Q"), with
  identical models numbered ("DELL U2720Q (2)").
- **Saved per user** in `%LOCALAPPDATA%\CupIsland\display.txt`, as the
  monitor's **device path**
  (`DisplayConfigGetDeviceInfo` → `monitorDevicePath`). That stays the same
  across reboots and replugging, unlike GDI names such as `\\.\DISPLAY2`.
  The app reads it before creating any window, so it starts on the saved
  monitor instead of starting on the main one and jumping.
- **Unplugged:** the island moves to the main display, but the pick is
  kept, so plugging the monitor back in brings the island back. The card
  shows where the island actually is ("Main display" meanwhile).
- **Any display change** (`WM_DISPLAYCHANGE`: plug, unplug, resolution,
  rearranging, a new primary) re-lists the monitors and re-places the
  island. That closes 017's open item.
- **Placement:** horizontally centred on the monitor, hanging from its top
  edge (`rcMonitor.top`, which isn't 0 for monitors above or below the
  primary). It uses that monitor's scaling: tokens are re-applied at its
  scale, as with a scale change (decision 017).

**Consequences.**
- The pick logic is pure, in `providers/system/settings_provider.cpp`
  (`placeIsland`, `refreshDisplays`), and tested in `tests/system_tests.cpp`.
- Enumeration is in `platform/displays.cpp`: `EnumDisplayMonitors`,
  `QueryDisplayConfig`, `GetDpiForMonitor`, all Windows 7/8.1, so fine
  for decision 015.
- The render layer has a `top`, and the interaction layer a full origin
  (`setRenderOrigin`).
- Fullscreen hiding already followed the island's own monitor, so it
  needed nothing.
