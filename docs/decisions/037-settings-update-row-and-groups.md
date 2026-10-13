# 037: Settings can install an update; every row is a list row; headings that read

Status: Accepted

**Context.** An update could only be installed from Home's Update widget:
Settings' Check for updates found one and said "0.5.1 on Home", sending the
user elsewhere to act on it. Copy logs and Centre island were two text
buttons side by side under rows that all looked and worked alike. And the
group names on Settings' pages ("Dark", "Light", "Anywhere") were captions:
the same small dim text as a row's value, easy to read past.

**Decision.**
- **Update and restart in Settings.** While an update is offered (found by
  Check for updates or by the daily check), a row under Check for updates
  installs it: the same `UpdateProvider::install()` as the Update card's
  button. Its value says the version, "Installing…", or why it failed
  ("Try the update again"). Check for updates now says "0.5.1 available".
  Settings learns of the offer from `UpdateProvider::onStatus`, told on
  every change (offered, installing, failed, withdrawn); the app connects
  the two, so neither provider knows the other.
- **Copy logs and Centre island are list rows**, like every other setting:
  the name, a glyph (Copy, and a new Centre), the whole row clickable,
  reached with the arrows like the rest. Copy logs' name says how the copy
  went, as its button did.
- **Headings stand out.** A new `TextStyle::Heading`: a little larger than
  caption, weight 700, in the title colour (`headingFontSize`,
  `headingFontWeight`). A heading that starts a later group has
  `sectionGap` of space above it (`Row::section`), so groups read as
  groups. Headings line up with the names under them (`Row::inset`: a list
  item's text inset, without its padding or hover).

- **Nothing cut short.** A row whose only stretching items are texts (a
  setting's name and its value) gives each its own width and the last the
  rest; short of room, each gives up a share in proportion. The even split
  before cut "Keyboard shortcuts" and "Check for updates" beside an empty
  or short value. The rows also use the card's whole width now
  (`listCardWidth` 340, was 280): narrowing them (decision 030) cost more
  in cut names than it gave in looks.

**Consequences.** The install row and headings in `tests/settings_tests.cpp`,
the status in `tests/update_tests.cpp`, the gap and inset in
`tests/content_tests.cpp`. The look needs a Windows run.
