# 051: The island in the playing cover's colours

Status: Accepted; a setting turns it off (052)

**Context.** While music plays, the island showed the same theme colours
whatever the cover looked like. People want it to feel like the song, the
way music apps tint their players. When nothing plays, or it's paused, the
island must look like the theme the user picked.

**Decision.**
- **One colour, not a gradient.** The island is one solid colour that fades
  to the next. Text is drawn against one known colour, so it stays readable,
  and a fill that only moves on a change costs nothing between changes. A
  gradient that moves all the time would re-render every frame while music
  plays: commit `0d153d8` fixed a stutter caused by exactly that. A second
  cover colour is also unreliable, since most covers are really one hue.
- **The cover's main colour** (`core/cover_palette.h`, `coverColor`): hues of
  the cover's coloured pixels in 15° bins, each pixel weighted by saturation
  and brightness, so a red logo on grey wins over the grey. Near-black, grey
  and see-through pixels don't count. A cover where fewer than
  `coverMinVivid` of the pixels are coloured (black-and-white, a grey
  podcast tile) has no colour, and the theme stays.
- **A theme built around it, in the theme's own recipe** (decision 022,
  `coverColors`). Only the cover's hue is used; lightness and chroma come
  from tokens, in OKLCH so "darker" doesn't drift a hue toward brown.
  A dark theme gets a tinted near-black island (`coverDarkIsland`), pale
  ink and the cover's colour as accent. A light theme gets a pale tint of
  it (`coverLightIsland`; not white, so each song still looks different)
  and dark ink. Every token keeps the theme's opacity. The shadow and
  `dangerColor` stay the theme's.
- **The provider says which colour, the host decides how.** `Content::tint`
  is set by `nowPlayingContent` while playing, with a coloured cover. The
  best-ranked activity with a tint colours the island (`core/island_tint.h`),
  so a Pomodoro in front of playing music doesn't stop it. Its own accents
  still win on its own content (decision 010).
- **Paused or stopped, the theme comes back after `coverTintLinger`**, so
  the moment between two tracks, when apps report "paused", doesn't flash
  the theme. A colour asked for shows at once.
- **Applied like a theme**: `app/live_tokens` puts the palette over the
  theme's colours, so `tokens()` stays what's on screen and a tokens.json
  save keeps it. Every shape fades to its new colour over
  `recolorDuration()`, the speed picked in Settings (decision 053) (`platform::fadeColor`), on a cover change and on a
  theme pick alike: the island's fill, the Split neck, the shadow, the tab,
  hover and tile marks, the focus ring, badges, and content shapes
  (progress bar and ring, equalizer bars, the knob, button and field
  backgrounds, the dim over clickable art). When only colours changed
  (`onlyColorsDiffer`), content is recoloured in place rather than rebuilt,
  so nothing fades out and back in; a new accent on one element fades too.
  Text and icons are pixels in a surface: they're redrawn in their new
  colours at once (a crossfade would need two surfaces per element).

**Consequences.** Tested in `tests/cover_tests.cpp`. That includes every
shipped theme around six hues: it keeps its lightness, and its text keeps
the theme test's contrast (title 7:1, body 4.5:1, caption 3:1). The look,
and the fade, need a Windows run.
