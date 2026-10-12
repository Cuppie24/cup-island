# 036: Releases from a workflow, updates from GitHub Releases

Status: Accepted

**Context.** Releasing meant editing `AppVersion` in the installer script,
building, tagging and uploading by hand. The script said 0.1.3 while the
release was v0.3.0, and the exe didn't know its own version at all. Users had
no way to learn about a new release except by checking GitHub. TR §10 allows
one kind of network call, an update check, and no update without the user
knowing. There's no server, and none is wanted.

**Decision.**
- **The tag is the version.** No file holds it. The Release workflow
  (`.github/workflows/release.yml`, run from the Actions tab with a version
  typed in) passes it to `build.ps1 -Version`, which hands it to CMake
  (`CUP_VERSION` → the generated `cup_version.h`: the code and the exe's
  version resource) and to Inno Setup (`/DAppVersion`). A local build's
  version is `git describe`'s: `0.3.0-5-gabc1234`, `-dirty` with local
  changes, `0.0.0-dev` without git. Only `X.Y.Z` is a release.
- **The workflow releases or does nothing.** Only from main; the version must
  be `X.Y.Z` and higher than every release tag. It builds and tests on
  `windows-latest`, makes the installer, and only then creates the tag and
  the release (`gh release create --target <sha> --generate-notes`), with
  the installer and `CupIsland.pdb` (Release builds now write one: `/Z7` +
  `/DEBUG`, so dumps from installed copies can be read, decision 013). A dry
  run stops before tagging; its installer is in the run's artifacts.
  Tagging and publishing are one job because a tag pushed with the
  workflow's own token starts no other workflow.
- **Every pull request is built on Windows** (`build.yml`: `build.ps1 -Test`
  on `windows-latest`, also on pushes to main). Linux sessions only build the
  core, and the first release run broke on a name clash in `main.cpp` that only
  MSVC could see; a PR now shows that before it's merged.
- **The updater** (`providers/update`): `UpdateProvider` is pure and tested;
  `github.cpp` is its Windows side.
  - A release build asks `api.github.com/repos/Cuppie24/cup-island/releases/latest`
    `updateFirstCheckDelayMs` after start, then every `updateCheckIntervalMs`
    (a day), counting from the last answered check, kept in the store, so
    restarts don't add checks. A failed check retries after `updateRetryMs`.
    Unauthenticated: 60 requests an hour per IP is plenty. The repo must stay
    public. Dev builds never check.
  - It offers a release only if it's newer and carries
    `CupIsland-Setup-<version>.exe` with a SHA-256 digest, both from
    `https://github.com/`. GitHub computes the digest when the file is uploaded.
  - An offer is an **Update widget** on Home whose card says what and from
    what, with **Install and restart** and **What's new** (the release page).
    It also posts **one alert per version**. Clicking the alert opens that card:
    a core rule, so an alert naming a panel with a card opens it
    (`alerts.scenario`).
  - **Install** downloads into `%LOCALAPPDATA%\CupIsland\updates`, rejects
    any download whose SHA-256 differs, runs the installer with
    `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART` (its log:
    `logs\update-install.log`), and quits. The installer's
    `skipifnotsilent` run entry starts the new island. Installers left behind
    are deleted at the next start. A failure is told on the card, with Try again.
  - Settings shows the running version under its list.

**Consequences.**
- Releasing is the Run workflow button; there's no version to edit anywhere.
- The checksum guards against a corrupted or tampered download, not against
  a compromised GitHub account; code signing (TR §10) would close that gap
  and silence SmartScreen.
- Copies installed before 0.4.0 have no updater: they update by hand once.
- Not verified on Windows from the Linux session that wrote it: the workflow's
  first dry run and the first real update are its tests.
