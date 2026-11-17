# 155: Windows' glass is Mica, not acrylic

Status: Accepted (amends 104).

**Context.** With Glass on, dragging a window and minimizing or maximizing it
felt laggy. Turning Glass off fixed it. With Glass for windows off and the
island's glass still on, the lag was gone too, so the cause was the windows'
backdrop, not the island's. Decision 104 gave providers' windows Windows'
acrylic (`DWMWA_SYSTEMBACKDROP_TYPE` = transient window) over the whole
window. Acrylic is a live blur of whatever is behind the window. When the
window moves, or animates on minimize or maximize, everything behind it
changes on every frame, so DWM blurs the whole window again on every frame.
Windows uses acrylic only for surfaces that are open briefly (menus,
flyouts). Its own main windows (Settings, Explorer, Terminal) use Mica.

**Decision.** Providers' windows take Mica (`DWMSBT_MAINWINDOW`) instead of
acrylic. Mica is tinted by the desktop wallpaper, blurred once by Windows
and redone only when the wallpaper or theme changes, so moving the window
costs nothing extra. Everything else in 104's "Glass for windows" stays:
the same switch, DWM's frame over the whole window, no system menu as
glass, the background see-through by `islandFill` at the Transparency pick,
light or dark by the windows' island colour, and solid before Windows 11
22H2 (which refuses the attribute).

(Not chosen: Mica Alt (`DWMSBT_TABBEDWINDOW`). It's tinted more strongly
and is meant for windows with tabs in the title bar. Keeping acrylic and
turning it off while the window moves (`WM_ENTERSIZEMOVE`): the window
would flicker solid on every drag, and minimize and maximize are DWM's
animations, which that message doesn't cover.)

**Consequences.**
- The windows show the wallpaper's colours, not the app behind them, as
  Windows' own apps do. That's less see-through than acrylic.
- Windows draws Mica only while the window is active; inactive, it shows
  its own solid fallback colour under our background. Expected Windows
  behaviour, not a bug.
- The fill was tuned over acrylic. If the windows now look too flat or too
  tinted, tune `glassPlainOpacity` (and `glassDensePlainOpacity`,
  `glassClearPlainOpacity` for the other Transparency picks) in
  `assets/tokens.json` (live). The island uses them too where Windows
  refuses its even brightness, so check the island after a change.
- No core change: the backdrop type lives in `platform/provider_window.cpp`,
  which only a Windows run shows.
