# 088: The service downloads no files

Status: Accepted; built, not yet run on Windows (narrows decision 081)

**Context.** Kaspersky removed v0.8.18's installer as
`VHO:Backdoor.Win32.Agent.gen`. The flagged file was `CupIslandIndex.exe`:
`data0002//data0000` is the service exe inside the service's installer,
inside the app's. `VHO` is a heuristic. The service looks like a backdoor to
one: it is unsigned, runs as SYSTEM, serves a named pipe, connects to the
internet, and can download an exe and run it. Decision 087's changes (it
writes an exe beside itself and starts it) most likely tipped the score.

The download has had no use for a while. Since 0.8.10, releases don't attach
the service's installer (decision 081), so the fallback that downloads it
never runs. It is still compiled in, though.

**Decision.**
- **The service installs only the installer the app brought.** It still
  copies the file into its own folder, locks it, and checks it against the
  release's hash (decision 081). If there is no copy, or the hash is wrong, it
  answers `failed` and logs why. It no longer falls back to a download.
- **WinHTTP stays, for one GET of the latest release's JSON.** That is where
  the trusted hash and version come from. The app can't be the source of the
  hash: then any process that can reach the pipe could have the service run
  its own exe as SYSTEM, which decision 076 rules out. Once the installers
  are Authenticode-signed, the service can check the signature instead, and
  this request can go too.
- `indexOfferFor` (core) is unchanged. It still fills in `url` for releases
  that attach the installer, and the service ignores it.

**Consequences.** The service binary no longer contains code that downloads a
file and runs it. A service updates only when the app's folder has the
matching installer, which every app install and update brings. Signing the
release binaries is still the real fix for antivirus heuristics, and this
change only lowers the score.
