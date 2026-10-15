#include "providers/media/now_playing.h"

#include "core/cover_palette.h"

namespace cup {

Content nowPlayingContent(const NowPlaying& m)
{
    namespace act = media_action;
    bool playing = m.timeline.playing;
    Image art{ m.art };
    Text title{ m.title, TextStyle::Title };
    Text artist{ m.artist, TextStyle::Body };

    Content c;
    c.label = m.artist.empty() ? m.title : m.title + " - " + m.artist;
    c.compactLeading = { row({ art }) };
    c.compactCenter = { row({ Text{ .value = c.label, .marquee = true } }) }; // swims when too long
    // Playing, the island takes on the cover's colours; paused, the theme's (decision 051).
    if (playing && m.art) c.tint = coverTint(*m.art);
    if (playing) c.backdrop = m.backdrop; // behind the island while it plays (decision 055)
    c.compactTrailing = { row({ Waveform{ playing } }, Align::Center) }; // paused: the bars rest as dots
    c.minimal = { row({ Waveform{ playing } }) }; // the Split bubble: equalizer bars, still dots when paused
    c.alertLeading = { row({ art }) };
    c.alertCenter = { row({ title }), row({ artist }) };

    // Home's widget (decision 019): the art on the left, the title (swimming
    // when long) with the bars, the artist under it.
    c.tileLeading = { row({ art }) };
    c.tile = { Row{ .items = { Text{ .value = m.title, .style = TextStyle::Title, .marquee = true } }, .trailing = { Waveform{ playing } } },
               row({ Text{ m.artist.empty() ? m.source : m.artist, TextStyle::Caption } }) };

    // Like iOS: the big art opens the app that's playing (only expanded; elsewhere a click expands).
    c.expandedLeading = { row({ Image{ m.art, m.canOpenSource ? act::kOpenSource : "" } }) };
    c.expandedCenter = { row({ title }), row({ artist }) };
    std::string caption = m.source, position = trackPosition(m.trackNumber, m.trackCount);
    if (!position.empty()) caption += (caption.empty() ? "" : " \u00B7 ") + position;
    if (!caption.empty()) c.expandedCenter.push_back(row({ Text{ caption, TextStyle::Caption } }));
    if (m.showSourceSwitch) c.expandedTrailing = { row({ Button{ Glyph::Source, act::kSource } }) }; // top-right corner

    if (m.timeline.duration > Ms{ 0 })
        c.expandedBottom.push_back(row({ TimeLabel{ m.timeline }, Progress{ m.timeline, m.canSeek }, TimeLabel{ m.timeline, true } }));
    Row controls = row({ Button{ Glyph::Previous, act::kPrevious, m.canPrevious },
                         Button{ playing ? Glyph::Pause : Glyph::Play, act::kPlayPause, m.canPlayPause },
                         Button{ Glyph::Next, act::kNext, m.canNext } },
                       Align::Center);
    controls.loose = true; // big, well-separated targets
    // Shuffle and repeat take the bottom corners; only shown when the app supports them.
    if (m.canShuffle) controls.leading.push_back(Button{ Glyph::Shuffle, act::kShuffle, true, m.shuffle });
    if (m.canRepeat)
        controls.trailing.push_back(Button{ m.repeat == Repeat::One ? Glyph::RepeatOne : Glyph::Repeat, act::kRepeat, true,
                                            m.repeat != Repeat::Off });
    c.expandedBottom.push_back(controls);
    return c;
}

Content nothingPlayingContent()
{
    Content c{ .label = "Nothing playing", .card = CardLayout::Centred };
    c.tileLeading = { row({ Icon{ Glyph::Music } }, Align::Center) };
    c.tile = { row({ Text{ "Not playing", TextStyle::Title } }), row({ Text{ "Play in any app", TextStyle::Caption } }) };
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
