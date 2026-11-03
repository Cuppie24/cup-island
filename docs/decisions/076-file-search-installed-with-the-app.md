# 076: File search is installed with the app, and updates itself

Status: Proposed (supersedes decision 075's opt-in "Turn on file search"
and its separately offered service updates; the rest of 075 stands).
Decision 078 moves its SearchSetupProvider into the host as ServiceHost.

**Context.** Decision 075 made file search opt-in: a Settings row ran the
service's admin installer, and a new service version was offered on a row
of its own. The owner wants search on by default, installed with the app
or with an update, and no setting for it. One admin installer for both was
considered and dropped: copies already installed (0.7.x) start the update
installer and quit at once (decision 036), so a refused or missed UAC
prompt would leave the island closed, and every later update would need a
prompt too, unless the service ran the app's installer, which would install
a per-user app as SYSTEM.

**Decision.**
- **The app's installer doesn't change** (decision 011: per user, no admin;
  decision 036: silent updates). It now carries the service's installer,
  `CupIsland-Index-Setup.exe`, beside `CupIsland.exe`.
- **The service has its own admin installer**, `CupIsland-Index-Setup-<v>.exe`
  (Inno Setup, `PrivilegesRequired=admin`), installing "Cup Island File
  Search" into Program Files with its own entry in Apps & Features. It
  stops an older service before copying (`CupIslandIndex.exe --stop`),
  then `--install`: creates or updates the service with its path quoted,
  starts with Windows, restarts after a crash (5 s, 30 s, 2 min; the count
  resets after a day), and starts it. Its uninstaller runs `--uninstall`
  and deletes the service's folder (index and log).
- **When it's first installed, with one UAC prompt either way:**
  - a fresh install: at the end of the app's setup, while the user is there;
  - an update from a copy without it: the new app's first start
    (`SearchSetupProvider`): an alert says Windows will ask, then the
    installer runs. Refused, the island carries on without search and asks
    again after its next update, never twice for one version.
  Dev builds have no bundled installer and never ask.
- **The service updates itself, silently.** After the app updates and
  restarts, it reads the service's version (the pipe's `status`); if it
  differs, it sends `update`. The request names nothing: the service asks
  GitHub for this repository's latest release itself, and installs only
  that release's `CupIsland-Index-Setup-<v>.exe` with its SHA-256 (the same
  rules as the app's updater, decision 040: any other version moves to the
  latest). A client can't pick a version, so it can't make the service
  install an old one with a known bug.
  - The download goes to the service's own folder (SYSTEM's LocalAppData,
    decision 075), which no user can write to; not `C:\Windows\Temp` or
    ProgramData, where any user can create files.
  - The file stays open, locked against writing and deleting, from download
    through the checksum to the installer running, so nothing can swap it
    in between. The installer's TMP and TEMP point into the same folder.
  - At most one update every 10 minutes, whatever clients ask.
- **One version, one release.** Both installers are built from the same
  commit by the same Release run, carry the same version, and are attached
  to the same GitHub release. Every release updates the service, even if
  its code didn't change: a few seconds without search, and nobody tracks
  which part changed. **Rule:** a change to a pipe message's shape bumps
  `kProtocolVersion` (`indexer/protocol.h`); the app works with the
  service's current and previous protocol, so an update the service hasn't
  finished yet breaks nothing.

**Consequences.** The only UAC prompt in Cup Island's life is the first
setup of search. Uninstalling the app leaves file search installed (the
app's uninstaller has no admin rights): it's removed in Apps & Features.
The service makes network calls of its own (GitHub, only when asked to
update), which TR §10's "update checks" covers. Code signing would let the
service check the installer's publisher instead of trusting GitHub's
checksum; until then the trust is the same as the app's updater's.

**Open.** Whether uninstalling the app should offer to remove search too
(a UAC prompt, and other Windows users may still use it).
