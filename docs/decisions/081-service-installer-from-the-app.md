# 081: The service updates from the installer the app brought

Status: Accepted (changes where decision 076's update gets its file; the
rest of 076 stands).

**Context.** A release page listed four files: the app's installer, the
service's installer and two pdbs. People downloaded the wrong one. The
service's installer was there only because the service downloaded it to
update itself (decision 076), checked against the SHA-256 GitHub shows for
an attached file. The app's installer already carries the same file
(`CupIsland-Index-Setup.exe` beside `CupIsland.exe`), and the download was
also the step most exposed to GitHub having a bad day.

**Decision.**
- **The release notes carry the hash.** The Release workflow writes
  `<!-- cup-island: index-sha256 <hex> -->` (an HTML comment, not shown),
  like the mandatory marker (decision 041). `indexHashIn` reads it;
  `indexOfferFor` takes it over the attached file's digest.
- **The app says where its copy is.** `service.update` gains an optional
  `installer` path (ServiceHostHooks::installerPath). It still names no
  version: the service asks GitHub which release is latest, as before.
- **The service decides by the hash alone.** It copies the file into its
  own folder (no user can write there), reopens the copy locked and hashes
  it through that handle (as 076), and runs it only if it matches the
  notes. A user can change the app's folder, but nothing they put there
  runs unless it's byte for byte what the release published.
  - Only a plain local path on a fixed drive (`plainLocalPath`, then
    `GetDriveType`): never `\\server\share`, where SYSTEM would sign in to
    whoever answers, nor a device, `\\?\` or stream path, nor a USB stick.
  - Copy wrong, missing or unreadable: it downloads the attached file, if
    the release still has one, under the same hash.
- **The pdbs live with the Release run's artifacts** (90 days, GitHub's
  most), not on the page. To read a crash dump, take them from the run that
  made that version (Actions → Release → its run → Artifacts).
- **One release of overlap.** Services from before this change only know how to
  download, so the first release with this still attaches the service's
  installer. The release after it drops it (one line in release.yml); from
  then on the page shows the app's installer alone.

- **When it's stuck, the user is told, with the download.** After
  serviceUpdateTries requests the service is still old (an old service with
  nothing to download, a bad copy, an installer that failed): a "File
  search needs an update" widget, an alert once per app version, and a
  "Download the installer" button opening the latest release page (the
  app's installer brings and sets up the service too). It goes as soon as
  the versions match. Likewise the app's own update, when it fails: its
  card adds "Download", the installer's link, beside "Try again".

**Consequences.** The service ends up on the version the app brought, with
no download; it can't update without a new app release, which is how
updates already worked (the app asks after it updates). A release made
without the workflow has no marker, so services fall back to the attached
file or stay put. A crash dump older than 90 days can't be symbolized.
