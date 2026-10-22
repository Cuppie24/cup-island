# 072: Inactive tabs are grey until hovered

Status: Accepted

**Context.** The tab row (decisions 007, 046, 047) showed every tab in full
colour: a cover, a coloured ring and bright titles in each. Only the
selected tab's faint background said which card was shown, so the row was
busy and the current tab hard to pick out.

**Decision.**
- **Everything in a tab that isn't the one shown is grey**: its title, glyph,
  picture (the cover thumbnail) and any ring, bar or waveform, and the
  whole tab at `tabMutedOpacity`. Home's button counts as a tab (grey
  unless Home is shown, a dormant panel's card included), and so does the
  pin while off; pinned, it's lit like the shown tab and in colour.
- **No background of its own**: a tab that isn't shown, Home's button and
  the pin while off used to sit on a faint `tileColor` background, always
  drawn. It's gone; with the grey it was noise. Only the shown tab's
  (`tabSelectedColor`, the pin's while pinned) and the hover remain. Home's
  tiles keep theirs.
- **Under the pointer** (or the keyboard's focus), a grey tab shows in
  colour at full strength, over `hoverFadeDuration`, on top of its usual
  hover background.
- **Core marks, platform draws.** Layout sets `Placed::muted` on a grey
  tab's items; which tab is hovered is the app's state, so
  `ElementVisuals::setHoveredTab` lifts it. Surfaces (text, glyphs,
  pictures) are drawn through Direct2D's Saturation effect at 0 (D2D 1.1,
  every Windows 10), so one path covers every kind of item; shapes (rings)
  fade their brushes to their colour's luminance grey. A picture crossfades
  to its new pixels; text is redrawn at once under the opacity fade.

**Consequences.** Hovering a tab redraws its few surfaces; nothing else
costs per frame. Links `dxguid` for the effect's CLSID.
