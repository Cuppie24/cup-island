# 062: Installs are per-user only; nothing starts the island elevated

Status: Accepted (amends 011)

**Context.** The installer was per-user by default but offered an
all-users install (`PrivilegesRequiredOverridesAllowed=dialog`). Two things
go wrong with an all-users install:
- **Silent updates stop being silent.** Writing to Program Files needs
  admin rights, so the in-app update (decision 036) shows a UAC prompt in
  the middle of a "silent" install.
- **The island comes back elevated.** After that elevated silent install,
  the `skipifnotsilent` run entry starts the new island with the
  installer's rights. OLE drag and drop doesn't cross integrity levels, so
  an elevated island gets no drops from a normal Explorer (decision 061),
  and nothing tells the user why. (The "Launch Cup Island" checkbox was
  already safe: `postinstall` entries run as the original user.)

**Decision.**
- Setup is per-user only: `PrivilegesRequired=lowest`, no override. No
  UAC prompt, ever, for install or update.
- The silent relaunch has `runasoriginaluser` too. It changes nothing for a
  per-user setup, but keeps the island unelevated if setup ever elevates
  again.
- **An existing all-users install** (its uninstall key under HKLM) stops
  setup before anything is changed. Otherwise a second, per-user copy would
  be installed next to it, with two Start menu entries.
  - Run by hand: setup offers to start the old uninstaller (it asks for
    admin rights itself) and asks to run setup again afterwards. Inno's
    uninstaller hands over to a temporary copy of itself and returns at
    once, so setup can't wait for it.
  - Silent (the in-app update): setup logs why and exits without changing
    anything. The running island is never closed, so it stays on its old
    version; a mandatory update (decision 041) uses up its tries the same way.

**Consequences.** `installer/cup-island.iss`: `[Setup]`, `[Run]` and an
`InitializeSetup` in `[Code]`. The island's own check at start (decision
061) catches being started elevated by anything else (an admin terminal,
say). Checked on Windows by: a fresh install, an update over a per-user
install, and setup over an old all-users install, both by hand and silent.
