# 040: The latest release is the one to run

Status: Accepted

**Context.** Versions 5.2.0 to 5.3.5 were released by mistake; the line
was 0.x (0.5.1 before them). Copies offered only a higher version (decision
036), and the Release workflow refused a version not higher than every
release tag, so there was no way back to 0.x: the mistake was permanent.
A one-time bridge for 5.x was considered and dropped: the next mistake
would need another.

**Decision.**
- **A copy runs GitHub's latest release, whatever its number**
  (`movesTo`, `providers/update/release`): it's offered whenever the latest
  is another version than the running one, higher or lower. A release made
  by mistake is undone by releasing the right one after it; a release that's
  pulled (deleted, so an older one is the latest again) is rolled back too.
  The saved offer (decision 039) follows the same rule.
- **The workflow** releases any `X.Y.Z` that doesn't exist yet. A version
  lower than an earlier release gets a warning in the run, so it's never by
  accident. Every release is created with `--latest`: GitHub picks "latest"
  by creation date anyway, but this doesn't depend on that.
- **Getting off 5.x:** copies on 5.2.0–5.3.5 still have the old rule
  (higher only), so they need one release they'll take that has this one:
  1. merge this and release **5.3.6**;
  2. wait a few days: copies check once a day (or Settings > Check for
     updates);
  3. release **0.6.0**. It's the latest, so every copy moves to it: 5.3.6
     ones down, 0.5.1 and older ones up.
  The 5.x releases and tags can stay or be deleted; nothing depends on them.
  A copy still on 5.2.0–5.3.5 after step 3 never sees 5.3.6 as the latest
  and is reinstalled by hand once (the installer goes over any version).

**Consequences.** Tested in `tests/update_tests.cpp`. A release can't be
"skipped" any more: whatever is published last, every copy installs. Dev
builds (`0.0.0-dev`, `0.5.1-3-g…`) still never check.
