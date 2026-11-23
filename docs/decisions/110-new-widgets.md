# 110: "New" on a widget until its card is first opened

Status: Superseded by 170 (no widgets to say "New" on). Core and scenarios tested; the saved list on Windows needs a run.

**Context.** The user asked for a "New" tag on Pomodoro's widget, once,
until they open it. A tag item exists (`Tag`, decision 048) and widgets
keep it (only buttons, fields and switches are left out of a widget,
decision 019), but tracking what's been seen was each provider's own work
(Settings does it for its rows). The user pointed out that "new until
opened" is the same for every widget: the host's job.

**Decision.**
- **The provider announces, the host does the rest.** `Panel::announce`, a
  number: 0, not announced; 1, this widget is worth announcing; raised
  later (2, 3...) to announce it again, when a feature is added to it: the
  provider decides (the user's ask). Not every widget the host hasn't seen
  opened: on the first run with this, every widget (theme, display,
  settings) would have said New at once, which says nothing.
- **Seen = its card opened since this announcement**
  (`IslandState::seenPanels`, id → the highest announcement opened):
  whenever an announced panel's card is expanded (its tile, its running
  activity's card, a page dot, a shortcut), its announce is recorded.
  `isNew`: announced, it has a card to open (not a toggle's tap or a static
  widget: nothing to open, so it would say New for ever), and its announce
  is higher than the one recorded.
- **Drawn by Home**: a `Tag{ "New" }` after the text of the widget's first
  line, as on a new setting (decision 048).
- **Kept by the app**: in the store under `island/seenPanels`, `<id> <n>`
  a line (a bare id reads as 1), loaded (`PanelsSeen`) before the providers
  publish their widgets, so a widget never flashes New after a restart;
  saved whenever an entry is added or raised.

**Not taken.** A provider keeping its own "seen" key (the first version):
the same logic in every provider that wants it.

**Consequences.** Pomodoro's panel announces itself (1). Announcing a
widget again is a one-number change in its provider.
