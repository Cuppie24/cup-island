# 180: The line being sung and the app's volume on media's card

Status: Accepted; core tested, the Windows side not yet run (amends 094, 119)

**Context.** Media's card showed what plays and its controls. Two things
were asked for: the song's words, the current line over the bar, and a
volume control beside the song's name. Decision 094 had left volume for
later: media sessions don't carry one.

**Decision.**
- **Lyrics come from LRCLIB** (lrclib.net): free, no key, and the only such
  service with timed lines. `providers/lyrics/` is pure: `lrc` reads the LRC
  text, `lrclib` builds the search and picks the record whose length is
  within `lyricsDurationTolerance` of the track's (another cut of the song
  would run out of step), `LyricsLookup` asks once per song and remembers
  the last `lyricsRemembered` answers, "none" included. A failed request is
  tried again when the song next comes up. `lrclib_fetch` is the request
  (WinRT, Windows only); it names the app and its version, nothing else.
  Asked only with a title, an artist and a length: a video has no artist,
  and without a timeline there's no place in the song to follow.
  It is part of the media provider (`MediaPresenter` owns the lookup), not
  a provider of its own: the line is on media's card, and providers don't
  know each other.
- **The host picks the line** (`Lyrics`, a new item): the provider sends
  the words once, with the timeline, and layout shows the line for `now`,
  as it does a TimeLabel's time. `Placed::changesAt` is the next line's
  start, one of `sceneDeadline`'s, so the app's one timer wakes for it.
  A note sign stands in before the first line and in a break.
- **It drops in** (`Text::drops`, `platform/dropping_text`): the new line
  falls in from above as the old one drops out below, clipped to its row,
  over `lyricDropDuration`. A rolling time's motion (decision 107), the
  whole line as one sprite.
- **A smaller cover makes the room**: `expandedArtSize` 108 to 84. The card
  stays `expandedSize.y`; the line, the bar and the controls share the
  height under the cover evenly (decision 119).
- **The row keeps its place between songs.** A song being looked up after
  one that had lyrics keeps the row, empty, so the bar doesn't jump at
  every track; it goes only when a song turns out to have none. The bottom
  rows are named (`Row::id`: lyrics, bar, controls), so the bar and the
  controls stay the same visuals as the line comes and goes.
- **Volume is the playing app's own**, the level Windows' volume mixer
  shows for it (`providers/media/app_volume`): every audio session of the
  app's processes, found by exe name or, for a Store app, by its app id.
  A speaker button mutes; beside it a bar sets the level
  (`Progress::level`: `levelBarWidth` wide, its own action, sent while it's
  dragged so the change is heard). The calls go to the audio service,
  another process, so they run on a thread of their own (decision 144).
  It's read when another app or track shows and when the card opens; a
  change made in the mixer meanwhile shows at the next of those.
  No audio session for the app (not played yet, or its id names no
  process): no control.

**Not taken.** The system's volume: the keyboard already has it, and the
card is about one app. Lyrics fetched by the service: it's a plain HTTPS
request as the user, like the update check. A setting to turn lyrics off:
worth adding with the first request for it; until then every song with an
artist is looked up, which sends its title and artist to lrclib.net.

**Consequences.** The cover is smaller on every Standard card, with or
without lyrics. The demo player (M) has made-up lyrics and a volume of its
own, to see both without a network.
