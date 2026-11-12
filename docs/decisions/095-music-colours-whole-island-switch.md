# 095: Music colours on the music's own surfaces, or the whole island

Status: Proposed (draft, not built: the Split pill and bubble may clash; see Open questions)

**Context.** With "Colours from music" on (decisions 051–055, and 090 as
glass), the whole island takes the playing cover's colours while music
plays, whatever it shows: a Pomodoro's pill, the to-do list, Settings. The
cover's colours say "this is the song", but on another provider's page they
say it about something that isn't the song.

**Proposal.**
- A new switch on the **Music colours** page: **"Apply to the whole
  island"**, **off by default**, under "Colours from music" (shown only
  while that one is on). Saved like the others (a new settings key).
- **On:** today's behaviour, unchanged: everything takes the cover's
  colours while music plays.
- **Off:** only the music's own surfaces take the cover's colours:
  - the media pill (Compact, while media is the shown activity);
  - the media Split bubble (media is the secondary activity);
  - the media alert (the track-change notification);
  - the expanded media card (and a floating media card).
  Every other provider's pill, bubble, alert and page uses the theme the
  user picked, even while music plays.
- Glass (decision 090) follows the same split: Midnight with the cover's
  glow on the music's surfaces, the plain glass theme elsewhere.

**Open questions (why it's a draft).**
- **Split mixes colours.** With another provider in the pill and media in
  the Split bubble, the pill shows the theme and the bubble the cover's
  colours, side by side and joined by the liquid neck (decision 007). That
  may look wrong. Options:
  1. Accept it: two activities, two looks (the neck fades between them).
  2. In Split, the bubble follows the pill (theme): the cover shows only
     when media owns the pill or the card.
  3. In Split, both follow the media: any Split with music takes the cover.
  Needs trying on screen before choosing.
- **Moving between surfaces.** Opening a card from a cover-coloured pill
  (or swiping to another provider's page) changes colours mid-morph: they
  should fade over `morphDuration`, with the shape, not the slower
  `recolorDuration` used for a new cover.
- **Text and accents.** Today they're recoloured with the island (tokens
  are global). With the switch off they need to follow the surface they
  sit on, so a theme-coloured page never gets cover-coloured text.

**How it could be built.** Today the palette is global: `app/live_tokens`
writes the cover's colours into `tokens()`. With the switch off, each blob
(pill, bubble, floating card) and the content drawn on it would need to
know whether it shows media and pick the cover palette or the theme's,
e.g. two token sets (theme and cover) with the surface choosing per frame
from the `Scene` (which activity owns the pill, the bubble, the card). That
reaches into every shape that reads a colour from `tokens()`, so it's a
large change; the decision on Split should come first.

**Consequences (if accepted).** People who update get the narrower
behaviour (off by default) and can turn "Apply to the whole island" on to
keep what they had. Tests: the switch in `tests/settings_new_tests.cpp`;
which surfaces take the cover in core (`island_tint`), per mode, in
`tests/scenarios/`.
