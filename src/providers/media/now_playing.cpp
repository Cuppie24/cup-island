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

    // Home's widget (decision 019): the art on the left, the title (swimming
    // when long) with the bars, the artist under it; a widget's sizes (decisions 111, 114).
    c.tileLeading = { row({ art }) };
    c.tile = { Row{ .items = { Text{ .value = m.title, .style = TextStyle::TileTitle, .marquee = true } }, .trailing = { Waveform{ playing } } },
               row({ Text{ m.artist.empty() ? m.source : m.artist, TextStyle::Body } }) };

    // Like iOS: the big art opens the app that's playing (only expanded; elsewhere a click expands).
    c.expandedLeading = { row({ Image{ m.art, m.canOpenSource ? act::kOpenSource : "" } }) };
    c.expandedCenter = { row({ title }), row({ artist }) };
    std::string caption = m.source, position = trackPosition(m.trackNumber, m.trackCount);
    if (!position.empty()) caption += (caption.empty() ? "" : " \u00B7 ") + position;
    if (!caption.empty()) c.expandedCenter.push_back(row({ Text{ caption, TextStyle::Caption } }));
    if (m.showSourceSwitch) c.expandedTrailing = { row({ Button{ Glyph::Source, act::kSource } }) }; // top-right corner

    if (m.timeline.duration > Ms{ 0 })
        // The times as wide as they get, so they sit close to the bar (decision 107).
        // The played part a wave, rippling while it plays and flat when paused (decision 167).
        c.expandedBottom.push_back(row({ TimeLabel{ .timeline = m.timeline, .fitted = true },
                                         Progress{ .timeline = m.timeline, .seekable = m.canSeek, .wavy = true },
                                         TimeLabel{ .timeline = m.timeline, .remaining = true, .fitted = true } }));
    // Three weights (decision 094): ▶/⏸ the filled circle, the skips bright,
    // shuffle and repeat quiet until on or pointed at.
    // Sized to fill the card's shared height (decision 107): the full ▶ circle, large skips.
    Row controls = row({ Button{ .glyph = Glyph::Previous, .actionId = act::kPrevious, .enabled = m.canPrevious, .large = true },
                         Button{ .glyph = playing ? Glyph::Pause : Glyph::Play, .actionId = act::kPlayPause,
                                 .enabled = m.canPlayPause, .primary = true },
                         Button{ .glyph = Glyph::Next, .actionId = act::kNext, .enabled = m.canNext, .large = true } },
                       Align::Center);
    controls.loose = true; // big, well-separated targets
    // Shuffle and repeat at the row's edges, under the ends of the bar's row: the transport
    // reads as one group, the modes as another (decision 168); ▶ stays centred.
    // Only shown when the app supports them.
    if (m.canShuffle)
        controls.leading.push_back(Button{ .glyph = Glyph::Shuffle, .actionId = act::kShuffle, .on = m.shuffle, .secondary = true });
    if (m.canRepeat)
        controls.trailing.push_back(Button{ .glyph = m.repeat == Repeat::One ? Glyph::RepeatOne : Glyph::Repeat, .actionId = act::kRepeat,
                                            .on = m.repeat != Repeat::Off, .secondary = true });
    c.expandedBottom.push_back(controls);
    return c;
}

Content nothingPlayingContent()
{
    Content c{ .label = "Nothing playing", .card = CardLayout::Centred };
    c.tileLeading = { row({ Icon{ Glyph::Music } }, Align::Center) };
    c.tile = { row({ Text{ "Not playing", TextStyle::TileTitle } }), row({ Text{ "Play in any app", TextStyle::Body } }) };
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
