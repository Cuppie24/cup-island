# 172: An update installs the latest release at the click

Status: Accepted (amends 036, 037, 041)

**Context.** Update and restart installed the offer as a check had found
it: the daily check's, or the one before a restart (decision 039), so
possibly days old. A release that came out after that check, even a
minute after it, wasn't installed: the copy moved to the older one, and
the next day's check offered the newer one again, a second download and
restart. The same went for a mandatory release installed at the next start.

**Decision.**
- **`UpdateProvider::install()` asks GitHub first.** It goes to Installing
  at once (Settings says "Installing…"), checks for the latest release, and
  installs that one, whatever the offer said: a later release, or a lower
  one if the offered one was pulled (decision 040). The answer counts as
  the day's check: it's saved as the offer, Settings shows its version, and
  a Check for updates waiting on it is told the same. A check already under
  way when the button is pressed answers for it; there's no second request.
- **GitHub can't be asked** (offline, rate-limited): the offer as found is
  installed. It's still a real release with its SHA-256, and its download
  isn't rate-limited like the API.
- **Nothing to move to any more** (the latest is this build, or has no
  usable installer): withdrawn, as after any check; nothing installs.
- **A mandatory install at start does the same** (decision 041), even if
  the latest isn't marked mandatory: a later release carries the required
  fix too. Its "Updating" alert waits for the answer so that it names the
  version that installs.
- The service already asks GitHub itself when it updates (decision 076),
  so it needs nothing.

**Consequences.** One more API request per install (an install is rare,
the limit is 60 an hour). Tested in `tests/update_install_tests.cpp`;
the install steps in `tests/update_tests.cpp` now answer the check first
(their shared fixture: `tests/update_fixture.h`).
