# 052: "Colours from music" is a setting

Status: Accepted; the row moved to a Music colours page and became a switch (053); off by default since 055

**Context.** Decision 051 recolours the island from the playing cover. Some
people want their theme to stay put whatever plays, so it needs a way off.

**Decision.**
- **A toggle on Settings' list**, under Appearance right after Theme:
  "Colours from music", a tick box like Start with Windows (a click anywhere
  on the row flips it). **On by default**, kept in the store
  (`covercolours`: "on"/"off"; nothing saved = on). It's an announced
  setting (decision 048), so people updating see it tagged "New".
- **On, the whole island follows the music**: Home, every card (Settings,
  Pomodoro), alerts, the pill and the bubble. Any page, as long as media
  plays (not paused). This is the one place a provider colours the whole
  island rather than its own content (decision 010 keeps accents to live data).
  It's a deliberate exception for media: the user picked it, and only
  `Content::tint` can do it, which only `nowPlayingContent` sets.
- **Off, the theme comes back at once**, without 051's pause linger: the
  user just asked for it, so there's nothing to smooth over. Turned on, the
  next dispatch picks up what's playing.
- The provider hands the choice to the app (`SettingsHooks::setCoverColours`),
  which stops asking for tints. Core's rules (`island_tint`) don't change.

**Consequences.** `tests/settings_new_tests.cpp` covers the default, the
toggle, its save and the restart. The look needs a Windows run.
