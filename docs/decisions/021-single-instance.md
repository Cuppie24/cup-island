# 021: One island at a time

Status: Accepted

**Context.** Nothing stopped a second copy from starting: launching from the
Start menu while the island ran, a "Start with Windows" entry plus a manual
start, or F5 while the installed copy ran. Each copy draws its own pill in the
same spot, so the islands stacked, and both handled clicks and media.

**Decision.** A named mutex,
`Local\CupIsland-6CB3203D-53EC-4816-944E-3BC5A4DD3489`
(`platform/single_instance.cpp`), is taken as the very first thing in
`main()`.
- The GUID is the installer's AppId, the app's permanent identity.
- `Local\` scopes it to the sign-in session: with fast user switching,
  each user still gets their own island.
- It's taken **before** the log is opened, so a turned-away copy never
  rotates or writes the running island's log.
- **A second launch** finds the running island's interaction window by class
  (`kInteractionWindowClass`), allows it to take the foreground, posts it the
  registered message `CupIsland.ShowYourself`, and exits. The running island
  opens Home, so the user sees it's already there. If its window doesn't
  exist yet (it's still starting), the second copy just exits. A Debug
  build prints "already running (pid N)" to its console.
- **Dev builds share the lock** with the installed copy: two islands are
  never useful. To run a dev build, quit the installed one from the tray.

**Consequences.**
- The mutex handle is never closed. Windows releases it when the process
  ends, crashes included, so a dead island can't block the next start.
- The installer already closes the running copy before upgrading
  (`CloseApplications=force`) and relaunches it afterwards, so upgrades
  are unaffected.
- All the APIs involved are in Windows 10 1809 (decision 015).
