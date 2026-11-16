# 107: Pomodoro's card: a header, a heavy time, steps long and short, rolling digits

Status: Accepted (amends 106: the card's layout; 010: how Steps are drawn). Core and provider tested; the look and the roll on Windows need a run.

**Context.** Decision 106 made the card a timer laid out like media's: the
ring where the cover goes, the time beside it in the light Display style, a
bar under them. The user asked for the card to look like a reference: the
phase and round in a header with the window's buttons, a big heavy time in
the middle, the session's phases as steps under it, long ones and short
ones, and the time's digits rolling as they change.

**Decision.**
- **The card is stacked and centred** (`CardLayout::Centred`), four rows:
  - *Header*: the phase's dot in its colour (a one-step `Steps`), its name
    (Title), the round in caption ("round 1 of 4", "paused · round 1 of 4",
    "last one" on the long break, "4 rounds" while dormant); at the right,
    statistics (`kOpenStats`), settings (`kOpenSettings`) and the window
    (`kOpenWindow`), secondary buttons. The first two open the window on
    their page (`WindowSpec::page`).
  - *The time left*, `TextStyle::Hero`: `heroFontSize` (64) at
    `heroFontWeight` (800), centred. Its separators (":") in the caption
    colour, its digits in the title colour. Display (the window's dial)
    stays as it was.
  - *The phases as steps* (`phaseSteps`): a focus a long pill, a break a
    dot, the long break a short pill, so how long each lasts reads at a
    glance. The window's dots are the same steps.
  - *The controls*: stop, ▶/⏸ (the full-size primary circle), skip. Dormant: ▶ alone.
  The ring and the bar are gone from the card: the steps say where the
  session is, the time says how much is left.
- **Steps have lengths** (`Steps::lengths`, `StepLength::Dot/Short/Long`;
  `stepShortWidth`, `stepLongWidth`). Missing lengths are dots, so other
  uses are unchanged. The current step gets a faint ring of its accent
  (`stepHaloWidth`, `stepHaloOpacity`); the frame keeps room for it on every
  side. A lone step (the header's dot) has none.
- **A Hero time rolls** (`platform/rolling_text`): each character is its own
  sprite in a container clipped to the line. A changed character's old
  sprite slides down a line and fades out (`morphEase`), the new one falls
  in from a line above and fades in, settling with `settleEase`'s little
  overshoot, over `digitRollDuration`. Unchanged characters stay. A change
  of length ("10:00" to "9:59") rolls every character. Reduced motion: they
  swap. Characters are placed where DirectWrite lays the whole line out
  (centred, tabular figures), each drawn alone, so no neighbour's ink
  bleeds into a rolling one; the visual reaches `rollingLead` past the frame
  on each side for a heavy glyph's overhang.

**Not taken.** Rolling every TimeLabel: small times tick beside bars, where
motion would distract. A stagger between rolling digits: the minute and
second rolling together reads as one tick.

**Consequences.** The card is a little taller than media's (it grows to
its rows, as a Centred card does), no longer the same size (106's "one size
by the same rule"). The roll is Composition-only and can't be checked in a
Linux session (decision 008).
