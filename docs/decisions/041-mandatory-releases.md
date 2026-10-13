# 041: Mandatory releases install themselves at the next start

Status: Accepted

**Context.** Some releases can't wait for each user to click Install: a
fix for a crash, or for the updater itself. Decision 036 (after TR §10)
installs nothing without a click.

**Decision.**
- **A release is mandatory when its notes say so**: the Release workflow's
  "Mandatory" checkbox writes a line ("Required update: …") and the marker
  `<!-- cup-island: mandatory -->` (an HTML comment, not shown on the page)
  before the generated notes. The updater reads the release's `body`
  (`isMandatory`); nothing else changes in how a release is found or checked.
- **Installed at the next start, no click.** A check that finds a mandatory
  release offers it as usual, and the card says "Required: installs at the
  next start". It's saved (decision 039, with a `mandatory` line); at the
  next start the provider restores it and installs it at once: download,
  SHA-256 check, silent installer, restart, as with the button.
- **The user still knows** (TR §10's intent): the card says beforehand that
  it will install, and at the start an alert says "Updating … a required
  update" while the widget shows it installing. A click is what's dropped,
  not the telling.
- **No loop on a broken installer:** at most 3 automatic tries per version
  (`mandatory-tries` in the store). After that it waits for a click like
  any release; Install still works. Installed (running that version), the
  offer and its tries are forgotten.
- Dev builds never check, so never install one.

**Consequences.** `tests/update_tests.cpp` (`mandatoryTests`). The marker
is written in `.github/workflows/release.yml` and matched in
`providers/update/release.h`: change both or neither. That `gh release
create` takes `--notes` with `--generate-notes` (the notes go first) is
gh's documented behaviour; the first mandatory release confirms it.
