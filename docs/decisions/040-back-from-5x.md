# 040: Back from 5.x, released by mistake

Status: Accepted (temporary: the rule goes once 5.x copies are back on 0.x)

**Context.** Versions 5.2.0 to 5.3.5 were released by mistake; the line
was 0.x (0.5.1 before them). The updater only offers a higher version
(decision 036), so a copy on 5.x would never move to a 0.x release, and the
Release workflow refuses a version not higher than every release tag.

**Decision.** A one-time bridge, in four steps:
1. **This rule, released as 5.3.6** (while the 5.x tags exist, so the
   workflow takes it): `movesTo` (`providers/update/release`) lets a copy on
   5.x take any release that isn't 5.x, lower included. Never from one 5.x
   to another lower one: while the 5.x releases are being deleted, GitHub's
   "latest" can be an older 5.x. Every other copy: only higher, as before.
   The restored offer (decision 039) uses the same rule.
2. **Wait a few days**: copies check once a day (or Settings > Check for
   updates), so the 5.x copies move to 5.3.6.
3. **Delete every 5.x release and tag** (5.2.0 to 5.3.6), for example
   `gh release delete v<version> --cleanup-tag -y` for each, and
   `git tag -d` locally. Then **release 0.6.0**: the workflow compares
   against v0.5.1. The 5.3.6 copies are offered 0.6.0 and install it; the
   installer doesn't refuse a lower version. Copies on 0.5.1 and older update
   as usual.
4. **Remove this rule and its tests** (`mistakenVersionTests`) in a later
   0.x release.

A copy that misses the window (still on 5.2.0–5.3.5 after step 3) never sees
a 5.x release again and has no rule to come back: it's reinstalled by hand
once; the installer goes over it.

**Consequences.** Tested in `tests/update_tests.cpp`. Major version 5 is
never used again, at least while the rule exists.
