# 030: Settings is a list, with a page of choices per setting

Status: Accepted (supersedes 022's and 023's ‹ › arrows)

**Context.** The Settings card picked the theme and the display with ‹ name ›
arrows. With eleven-plus themes that's a lot of clicking to find one, and
the arrows don't show what there is to choose from, or which themes are
light. Start with Windows was in the tray menu, which few people open, and
was off until someone found it. And a click on the card between its
controls closed it, like every card: on a page of settings, a near miss
threw the user out.

**Decision.**
- **The card is a list of settings**, each a list item (`Row::listItem`,
  decision 020) that's clickable across its whole row: Theme and Display
  show their value and a chevron, Start with Windows a checkbox. Copy logs
  and Centre island stay as buttons under it.
- **Theme and Display open a page of choices**, one shared look (a select):
  a title with a back arrow, then every choice as a list item, the current
  one ticked. A pick applies, is saved and goes back to the list; the back
  arrow changes nothing. The page is a host list (`Row::list`, decision 029),
  so it scrolls, and it opens scrolled to what's picked (`Host::reveal`).
  The card stays one card: pages are what the provider publishes, so
  nothing new in the host. The page isn't reset when the card closes (the
  provider isn't told); the back arrow is one click.
- **Themes are grouped**, Dark then Light, under captions in the same list.
  Which group is derived, not declared: `Theme::light` is set when
  tokens.json is parsed, from the brightness of the island the theme ends
  up with (a theme that keeps the file's `islandColor` is as light as the
  file). A new theme needs no extra field. So a list can open with a
  shorter heading, the list window is `listVisibleRows` of its *tallest*
  row, not its first.
- **More light themes**: Mint, Lavender, Sand and Sky, built like Paper and
  Sorbet (decision 022): a pale island, dark ink, the accent for fills and
  faint tints. The contrast test covers them.
- **Start with Windows is on by default**, and it's on the card, not the
  tray (the tray keeps Exit). The first run turns it on and saves "on" in
  the provider's store (`autostart`); after that the user's choice stands,
  so turning it off (here, or Task Manager) sticks. The row reads the Run
  key each time it's shown. This changes autostart.h's "opt-in only".
- **A card can stay open on a click** (`Content::keepOpen`): a click that
  lands between its controls does nothing. Esc, a click outside the island
  and the idle timeout still close it. Settings sets it; other cards keep
  closing on a click.

**Consequences.** Card building in `providers/system/settings_card.cpp`,
logic in `settings_provider.cpp`, both tested in `tests/system_tests.cpp`;
`keepOpen` in `tests/scenarios/home.scenario`; the grouping in
`tests/tokens_tests.cpp`. The look of the list and the pages needs a
Windows run.
