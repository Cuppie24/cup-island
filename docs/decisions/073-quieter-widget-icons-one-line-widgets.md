# 073: Quieter widget icons, one-line widgets

Status: Accepted

**Context.** On Home, a widget's icon (the theme's sun or moon, Settings'
gear, the update's download arrow) was drawn as strong as its text. The theme
and Settings widgets put the icon on a line of its own over a single word,
unlike the media and timer widgets, which keep everything to their rows of
text. Settings' word was body text, while every other widget's main line is
a Title.

**Decision.**
- **Widget icons are quiet:** Home marks every `Icon` in a widget
  `Icon::quiet`, drawn in `buttonOffColor`, the grey of a secondary button
  at rest (decision 064). Providers don't set it, and icons elsewhere
  (tabs, compact) keep `iconColor`.
- **One line where there's one thing to say:** the theme and Settings
  widgets are one row, the icon beside its word, centred in the tile like
  the other widgets' rows.
- **Settings' word is a Title**, bold like every other widget's main line.

**Consequences.** A widget with a lone icon line and a text line under it
reads as two things; providers making a new one-word widget should put the
icon in the same row.
