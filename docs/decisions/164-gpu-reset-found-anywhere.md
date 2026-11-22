# 164: A GPU reset is recovered wherever it shows first

Status: Accepted. Needs a Windows run after a GPU reset (a game's driver
timeout, a driver update).

**Context.** During a game, the graphics driver reset: the monitor came
back as "Display 1" at 150% for a few seconds. Afterwards every content
build logged `WinRT error building content: 0x887a0005` (DXGI device
removed) and the island stayed broken until it was restarted. The log
never said `graphics device lost, recreating`: the device was only
checked in `GraphicsDevice::draw`, at `BeginDraw`. But a content build
creates a new element's surface before drawing it, and
`CreateDrawingSurface` on the removed device threw first, every time, so
the check that rebuilds the device was never reached.

**Decision.** `createSurface` and `EndDraw` recover too: a device-lost
error rebuilds Composition's rendering device (`recover()`), and
`createSurface` then tries once more. `generation()` goes up on each
rebuild, and `ElementVisuals::apply` repaints every surface when it
changed during the apply, since a reset found in `createSurface` returns
no `false` the way `draw` does.

**Consequences.** Provider windows and the glass backdrop keep their own
surfaces: after a reset they repaint at their next update, not at once.
