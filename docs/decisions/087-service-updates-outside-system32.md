# 087: The service updates from its install folder, not System32

Status: Accepted; built, not yet run on Windows (fixes decisions 076 and 081)

**Context.** A 0.8.15 service never moved to 0.8.16. Its log showed six
tries that each got as far as "installer started" (the app's copy matched
the release's hash, and `CreateProcess` succeeded). After that: no
`stopped` line, so the installer never reached its `--stop`, and no
`index-update.log`, so it quit before writing its own log. The same
installer, run by hand from an admin terminal, updated the service at once.

The difference is the paths. The service gave the installer its data
folder: the installer's TMP/TEMP, its working folder and `/LOG=` were all
under SYSTEM's LocalAppData, which is
`C:\Windows\System32\config\systemprofile\AppData\Local\...`. Inno Setup's
setup.exe is a 32-bit program (`ArchitecturesInstallIn64BitMode` only
changes where it installs, not what it runs as), and WOW64 redirects a
32-bit process's `System32` paths to `SysWOW64`. There those folders don't
exist, so it had nowhere to unpack itself and quit. Run by hand, TEMP is
the user's own and nothing is redirected.

The fallback didn't help either. The "File search needs an update" card
sends people to the app's installer, and that installer only set up the
service when it *wasn't* installed. So it updated the app and left the old
service in place.

**Decision.**
- **Self-updates work in the install folder.** That's
  `C:\Program Files\Cup Island File Search` (`installFolder()`, from the
  exe's own path). Installers go in `updates\` there, and the installer's
  log is `index-update.log` beside the exe. Program Files isn't redirected
  for 32-bit programs, and only administrators can write to it. That
  keeps 076's rule: no user can plant files in the installer's temp folder.
  The data folder (index snapshots, `index.log`) stays where it was. The
  uninstaller removes both new paths. At start the service also clears the
  `updates\` folder older services left in the data folder.
- **The next try says how the last one ended.** The service can't wait for
  its installer, because the installer stops it. So it keeps the process
  handle, and the next update request logs the exit code. A run of failed
  tries then reads "exited with code N", not the same "installer started"
  line over and over.
- **The app's installer updates an older service.** An interactive setup
  runs the service's setup when the service is missing *or* its uninstall
  entry's `DisplayVersion` isn't this version. Same version installed: no
  second permission prompt, as before. Silent (in-app) updates still never
  run it.

**Consequences.** A service at 0.8.16 or older carries the old paths, so it
can't update itself to the fixed one. Those PCs get the fix once, by hand:
the "Download the installer" card now works for that. From the fixed
release on, self-updates should go through. If one doesn't,
`index.log` now names the exit code and `index-update.log` exists.
