# 017: Display scaling: tokens are logical pixels

Status: Accepted

**Context.** The app is per-monitor DPI aware (so Windows doesn't blur it by
stretching a bitmap), but laid everything out in raw pixels: on a 150%
display the island was two-thirds the size of Windows' own UI around it.
TR §9 asks for correct scaling per display.

**Decision.**
- **tokens.json is in logical pixels**, the size at 100% scaling. When it's
  loaded, the app multiplies every length by the primary display's scale
  (`scaleTokens`): sizes, the canvas, paddings, gaps, radii, font sizes,
  drag distances, speeds in px/s. Durations, counts, curves, colours and
  the two press factors (`pressScale`, `buttonPressScale`) stay as they are.
- Everything downstream (layout, surfaces, text, the windows, hit-testing)
  then simply works in physical pixels. Nothing is drawn small and
  stretched, so text and art stay crisp, and pointer coordinates need no
  conversion. Core stays unaware of DPI; its tests run at scale 1.
- **`islandScale`**, one size knob (default 0.85): lengths are multiplied
  by the display's scale times it. Windows' scaling alone made the island
  feel too big next to the taskbar and tray; one factor keeps every
  proportion while making it more compact. Tunable live: a changed canvas
  resizes the render layer.
- **The island's own display's scale is used** (`GetDpiForMonitor`): the
  primary display at first, any display since decision 023.
- **Changing the scale while it runs** (Settings › Display › Scale, or a
  new primary display) arrives as `WM_DPICHANGED`: tokens.json is
  re-applied at the new scale, the render layer is resized and re-centred,
  and the content is rebuilt. This is the same path as a tokens.json
  reload, plus the canvas resize a reload never does.

(Not chosen: a Composition scale transform on the root visual. It would
need surfaces drawn at a higher DPI under it, a counter-scale on the
swimming-text brush, and pointer conversion; scaling the numbers once is
simpler and can't drift out of sync.)

**Consequences.** `core/tokens_json` (`scaleTokens`, tested), `app/live_tokens`
(`setDisplayScale`, `rescaleTokens`), `platform/render_layer`
(`resizeRenderLayer`; the scale now comes from `platform/displays`), and WM_DPICHANGED in the
interaction layer. Values in tokens.json don't change; at 100% nothing
looks different.

**Crisp text.** Scaled lengths are fractional, which blurs text unless
visuals are snapped to whole pixels. That, and the type ramp, weights and
text rendering, are in decision 024.

**Resolved (decision 023).** The island can be put on any display from
Settings, at that display's scale. Any display change, a resolution change
included, re-places it.
