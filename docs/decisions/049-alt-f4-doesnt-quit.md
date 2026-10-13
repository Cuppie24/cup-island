# 049: Alt+F4 doesn't quit the island

Status: Accepted

**Context.** The island takes keyboard focus when it's clicked (to type, to
use its keys). Alt+F4 then went to it like to any focused window, and
Windows closed it: the app quit. People press Alt+F4 to close the window
they were just in, and the island is easy to have clicked last.

**Decision.** The interaction window ignores `WM_SYSCOMMAND` with
`SC_CLOSE`, which is what Alt+F4 (and only a window menu, which the island
has none of) sends. Nothing else changes: the tray's Exit (the
one way to quit by hand; Esc only steps back, decision 033), the updater (`DestroyWindow`), the installer (Restart
Manager, then `taskkill /f`) and shutdown don't come through `SC_CLOSE`.

**Consequences.** One case in `interaction_layer.cpp`'s window procedure;
needs a Windows run to see. Alt+F4 can still be recorded as a shortcut
(decision 032): `WM_SYSKEYDOWN` reaches the app before the command does.
