# 110: "New" on a widget until its card is first opened

Status: Accepted. Core and scenarios tested; the saved list on Windows needs a run.

**Context.** The user asked for a "New" tag on Pomodoro's widget, once,
until they open it. A tag item exists (`Tag`, decision 048) and widgets
keep it (only buttons, fields and switches are left out of a widget,
decision 019), but tracking what's been seen was each provider's own work
(Settings does it for its rows). The user pointed out that "new until
opened" is the same for every widget: the host's job.

**Decision.**
- **The provider announces, the host does the rest.** `Panel::announce`:
  this widget is worth announcing. Not every widget the host hasn't seen
  opened: on the first run with this, every widget (theme, display,
  settings) would have said New at once, which says nothing.
- **Seen = its card opened, ever** (`IslandState::seenPanels`): whenever a
  panel's card is expanded (its tile, its running activity's card, a page
  dot, a shortcut), its id goes in, once. `isNew`: announced, it has a card
  to open (not a toggle's tap or a static widget: nothing to open, so it
  would say New for ever), and not seen.
- **Drawn by Home**: a `Tag{ "New" }` after the text of the widget's first
  line, as on a new setting (decision 048).
- **Kept by the app**: in the store under `island/seenPanels`, one id a
  line, loaded (`PanelsSeen`) before the providers publish their widgets,
  so a widget never flashes New after a restart; saved when the list grows.

**Not taken.** A provider keeping its own "seen" key (the first version):
the same logic in every provider that wants it.

**Consequences.** Pomodoro's panel announces itself. A provider that wants
to announce a widget again (a new feature in it) needs a new panel id, or a
way to clear its entry, which there isn't yet.
