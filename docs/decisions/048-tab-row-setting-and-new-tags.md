# 048: The tab row's place is a setting; new settings say "New"

Status: Accepted

**Context.** Decision 047 moved the tab row to the top; some will want it
back at the bottom. And a setting added in an update is easy to miss: nothing
says it's there, let alone that it's on a page under another row.

**Decision.**
- **Tab row** (Settings, Appearance): a page of Top and Bottom. Picking one
  moves the row at once (the page stays: the card behind it is the preview)
  and is kept in the store (`tabrow`). Core state `IslandState::tabsOnTop`,
  set with `TabRowPlaced`; at the bottom the content keeps its place and the
  strip adds room under it, as before 047.
- **Announced settings** (`announcedSettings()`): every setting added after
  people had the app, by an id that never changes, with its parent (the row
  whose page it's on; "" for the list). Its row carries the id
  (`Row::id`); add a line when adding a setting.
- **"New":** an announced setting not seen yet has a `Tag` ("New", a pill in
  `tagColor`) right after its name. Its parent's row gets a dot on its
  chevron (`Button::badge`) while anything on its page is unseen, and the
  Settings widget and tab get one (`Content::badge`) while anything is.
- **Seen = shown, then left:** while the card is on screen
  (`Provider::onCardShown`, new), every announced row it shows is being
  seen; when the card closes they're seen (`seen` in the store), so
  re-entering shows no tag. A setting on a page that wasn't opened was never
  shown: its tag and its parent's dot stay.
- **A fresh install has seen everything** (nothing saved yet: every setting
  is as new as the others); an update from before tags (other settings
  saved, no `seen`) sees the announced ones as new.

**Consequences.** `tests/settings_new_tests.cpp`, a `tabrow` line in
`tests/scenarios/pin.scenario`. `Tag` is a new content item (PS §5 subset,
decision 004): painted as a pill with centred text.
