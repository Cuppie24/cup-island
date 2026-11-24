#include "providers/media/now_playing.h"

#include "core/cover_palette.h"

namespace cup {

Content nowPlayingContent(const NowPlaying& m)
{
    namespace act = media_action;
    bool playing = m.timeline.playing;
    Image art{ m.art };
    // Big and heavy beside the cover; too long, it swims like the pill's title instead of ending in "…" (decision 107).
    Text title{ .value = m.title, .style = TextStyle::CardTitle, .marquee = true };
    Text artist{ m.artist, TextStyle::Body };

    Content c;
    c.label = m.artist.empty() ? m.title : m.title + " - " + m.artist;
    c.compactLeading = { row({ art }) };
    c.compactCenter = { row({ Text{ .value = c.label, .marquee = true } }) }; // swims when too long
    // Playing, the island takes on the cover's colours; paused, the theme's (decision 051).
    if (playing && m.art) c.tint = coverTint(*m.art);
    c.tintComing = playing && !m.art; // a new track's cover is still being read (decision 058)
    c.compactTrailing = { row({ Waveform{ playing } }, Align::Center) }; // paused: the bars rest as dots
    c.minimal = { row({ Waveform{ playing } }) }; // the Split bubble: equalizer bars, still dots when paused
    c.alertLeading = { row({ art }) };
    // The alert is a short banner, not a card: the dense Title like every other alert (design rules).
    c.alertCenter = { row({ Text{ .value = m.title, .style = TextStyle::Title, .marquee = true } }), row({ artist }) };

    // Like iOS: the big art opens the app that's playing (only expanded; elsewhere a click expands).
    c.expandedLeading = { row({ Image{ m.art, m.canOpenSource ? act::kOpenSource : "" } }) };
    c.expandedCenter = { row({ title }), row({ artist }) };
    // The app's volume while its speaker button (in the controls' row) has it open
    // (decision 180): a bar that sets the level, on a pill floating right above the speaker.
    if (m.volume && m.volumeOpen)
        c.popover = Popover{ act::kVolumeOpen, row({ Progress{ .timeline = levelTimeline(m.muted ? 0.0f : *m.volume), .seekable = true, .level = true,
                                                               .actionId = act::kVolume } }) };
    std::string caption = m.source, position = trackPosition(m.trackNumber, m.trackCount);
    if (!position.empty()) caption += (caption.empty() ? "" : " \u00B7 ") + position;
    if (!caption.empty()) c.expandedCenter.push_back(row({ Text{ caption, TextStyle::Caption } }));
    if (m.showSourceSwitch) c.expandedTrailing = { row({ Button{ Glyph::Source, act::kSource } }) }; // top-right corner

    // The line being sung, over the bar (decision 180).
    // The bottom rows are named (Row::id), not numbered: the line comes and goes
    // with the song, and the bar and controls under it stay the visuals they were.
    if (m.lyrics || m.lyricsPending) {
        c.expandedBottom.push_back(row({ Lyrics{ m.timeline, m.lyrics } }));
        c.expandedBottom.back().id = "lyrics";
        c.smallCover = true; // the cover gives the line its room; without one it's full size
    }
    if (m.timeline.duration > Ms{ 0 }) {
        // The times as wide as they get, so they sit close to the bar (decision 107).
        // The played part a wave, rippling while it plays and flat when paused (decision 167).
        c.expandedBottom.push_back(row({ TimeLabel{ .timeline = m.timeline, .fitted = true },
                                         Progress{ .timeline = m.timeline, .seekable = m.canSeek, .wavy = true },
                                         TimeLabel{ .timeline = m.timeline, .remaining = true, .fitted = true } }));
        c.expandedBottom.back().id = "bar";
    }
    // Three weights (decision 094): ▶/⏸ the filled circle, the skips bright,
    // shuffle and repeat quiet until on or pointed at.
    // Sized to fill the card's shared height (decision 107): the full ▶ circle, large skips.
    Row controls = row({ Button{ .glyph = Glyph::Previous, .actionId = act::kPrevious, .enabled = m.canPrevious, .large = true },
                         Button{ .glyph = playing ? Glyph::Pause : Glyph::Play, .actionId = act::kPlayPause,
                                 .enabled = m.canPlayPause, .primary = true },
                         Button{ .glyph = Glyph::Next, .actionId = act::kNext, .enabled = m.canNext, .large = true } },
                       Align::Center);
    controls.loose = true; // big, well-separated targets
    controls.id = "controls";
    // Two pairs at the row's edges, mirrored (decisions 168, 180): at the very ends the speaker
    // that opens the volume's bar and the lyrics toggle, inside them shuffle and repeat. All four
    // the same quiet toggles; the transport stays centred between them. Shuffle, repeat and the
    // speaker only where they apply (the app's modes, a known volume). The lyrics button stays
    // with a song that has no lyrics, disabled: it says so, and keeps the row's end in place.
    if (m.volume)
        controls.leading.push_back(Button{ .glyph = m.muted || *m.volume <= 0 ? Glyph::Muted : Glyph::Volume, .actionId = act::kVolumeOpen,
                                           .on = m.volumeOpen, .secondary = true });
    if (m.canShuffle)
        controls.leading.push_back(Button{ .glyph = Glyph::Shuffle, .actionId = act::kShuffle, .on = m.shuffle, .secondary = true });
    if (m.canRepeat)
        controls.trailing.push_back(Button{ .glyph = m.repeat == Repeat::One ? Glyph::RepeatOne : Glyph::Repeat, .actionId = act::kRepeat,
                                            .on = m.repeat != Repeat::Off, .secondary = true });
    if (m.lyricsOn)
        controls.trailing.push_back(Button{ .glyph = Glyph::Lyrics, .actionId = act::kLyrics, .enabled = !m.lyricsMissing,
                                            .on = *m.lyricsOn && !m.lyricsMissing, .secondary = true });
    c.expandedBottom.push_back(controls);
    c.noIdleTimeout = true; // kept open while listening: left alone, it stays (decision 177)
    return c;
}

Content nothingPlayingContent()
{
    // Stays when left alone too, so the card doesn't close when the music ends (decision 177).
    Content c{ .label = "Nothing playing", .card = CardLayout::Centred, .noIdleTimeout = true };
    c.expandedCenter = { row({ Icon{ Glyph::Music } }, Align::Center),
                         row({ Text{ .value = "Nothing playing", .style = TextStyle::Title, .centred = true } }),
                         row({ Text{ .value = "Music and videos from any app show here", .style = TextStyle::Caption, .centred = true } }) };
    return c;
}

std::string trackPosition(int number, int count)
{
    if (number <= 0) return "";
    std::string text = "Track " + std::to_string(number);
    if (count >= number) text += " of " + std::to_string(count); // some apps send a count of 0 or garbage
    return text;
}

Repeat nextRepeat(Repeat current)
{
    switch (current) {
    case Repeat::Off: return Repeat::All;
    case Repeat::All: return Repeat::One;
    case Repeat::One: return Repeat::Off;
    }
    return Repeat::Off;
}

} // namespace cup
