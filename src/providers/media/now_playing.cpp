#include "providers/media/now_playing.h"

#include "core/cover_palette.h"

#include <algorithm>
#include <cmath>

namespace cup {

namespace {

Content cardContent(const NowPlaying& m);

} // namespace

Content nowPlayingContent(const NowPlaying& m)
{
    return m.lyricsPage && m.lyrics ? lyricsPageContent(m) : cardContent(m);
}

Content lyricsPageContent(const NowPlaying& m)
{
    namespace act = media_action;
    NowPlaying card = m;
    card.lyricsPage = false;
    Content c = cardContent(card); // the pill, the bubble, alerts and the tint don't change with the page
    c.card = CardLayout::Centred;
    c.smallCover = false;
    c.expandedLeading.clear();
    c.expandedTrailing.clear();
    c.expandedBottom.clear();
    c.backAction = act::kLyricsBack; // Esc goes back to the card, as the arrow does
    // The words are the whole page, from its top. The arrow and the sync button float over its
    // top corners (Content::overTop): the line being sung is in the middle, never under them.
    Row heading;
    // Toggles both, so both have a toggle's circle and, on liquid glass, its shiny edge. The
    // sync button's is lit while the page follows the song; the arrow's never is.
    heading.leading.push_back(Button{ .glyph = Glyph::ChevronLeft, .actionId = act::kLyricsBack, .on = false, .compact = true, .secondary = true });
    heading.trailing.push_back(Button{ .glyph = Glyph::Sync, .actionId = act::kLyricsSync, .on = m.lyricsSynced, .compact = true, .secondary = true });
    heading.id = "heading";
    c.overTop = { heading };
    c.expandedCenter.clear();
    // The card keeps its height (these rows ask for less) and the list fills it, from the top.
    c.sizedLike = { heading };
    c.pageAtTop = true;
    // Empty rows before the first line and after the last, about half the page's height: the
    // song's first and last lines can be in the page's middle too, as every other is when sung.
    auto margin = [&](const char* end) {
        for (int i = 0; i < kLyricsPageMargin; ++i) {
            Row blank = row({ Text{ .style = TextStyle::Title } }); // three of these: just under half the page
            blank.list = act::kLyricsList;
            blank.visibleRows = -1;
            blank.id = end + std::to_string(i);
            c.expandedCenter.push_back(blank);
        }
    };
    margin("before");
    for (std::size_t i = 0; i < m.lyrics->size(); ++i) {
        bool sung = static_cast<int>(i) == m.lyricsLine;
        const std::string& words = (*m.lyrics)[i].text;
        // Big text, five lines to the page; the one being sung bigger still and bright. A line with no words is a break: a note in its
        // place, as on the card, so the list keeps the song's shape.
        Text text{ .value = words.empty() ? kLyricBreak : words, .style = sung ? TextStyle::LyricSung : TextStyle::Lyric, .centred = true,
                   .wrap = !words.empty() };
        Row shown = row({ text });
        shown.list = act::kLyricsList;
        shown.visibleRows = -1; // the list is the rest of the card
        shown.id = "line" + std::to_string(i); // named: a row keeps its visual as the lit one moves on
        c.expandedCenter.push_back(shown);
    }
    margin("after");
    return c;
}

std::size_t lyricsPageRow(const NowPlaying& m)
{
    if (!m.lyrics || m.lyrics->empty()) return 0;
    return kLyricsPageMargin + std::min(static_cast<std::size_t>(std::max(m.lyricsLine, 0)), m.lyrics->size() - 1); // past the empty rows at its top
}

namespace {

Content cardContent(const NowPlaying& m)
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
    // With lyrics, the pill shows the line being sung instead, and is wider so it fits
    // (decision 180); a line still too long ends in "…". Paused, the song's name again.
    if (m.lyrics && playing) {
        c.compactCenter = { row({ Lyrics{ .timeline = m.timeline, .lines = m.lyrics } }) };
        c.widePill = true;
    }
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
    std::string caption = m.source, position = trackPosition(m.trackNumber, m.trackCount);
    if (!position.empty()) caption += (caption.empty() ? "" : " \u00B7 ") + position;
    if (!caption.empty()) c.expandedCenter.push_back(row({ Text{ caption, TextStyle::Caption } }));
    if (m.showSourceSwitch) c.expandedTrailing = { row({ Button{ Glyph::Source, act::kSource } }) }; // top-right corner

    // The line being sung, over the bar (decision 180).
    // The bottom rows are named (Row::id), not numbered: the line comes and goes
    // with the song, and the bar and controls under it stay the visuals they were.
    if (m.lyrics || m.lyricsPending) {
        // Clicked, it opens the lyrics' page; while they're still looked up there's nothing to open.
        c.expandedBottom.push_back(row({ Lyrics{ .timeline = m.timeline, .lines = m.lyrics, .actionId = m.lyrics ? act::kLyricsPage : "", .italic = true } }));
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
    // Two pairs at the row's edges, mirrored (decisions 168, 180): at the very ends the volume's
    // speaker and the lyrics toggle, inside them shuffle and repeat. All four
    // the same quiet toggles; the transport stays centred between them. Shuffle, repeat and the
    // speaker only where they apply (the app's modes, a known volume). The lyrics button stays
    // with a song that has no lyrics, disabled: it says so, and keeps the row's end in place.
    // The speaker mutes. With the pointer on it, it opens to its right into the volume's bar,
    // over shuffle's place, the two on one pill that grows out of it (Row::leadingOpen); only
    // the speaker lights up under the pointer, so it reads as the button. Shuffle is back when
    // the pointer leaves the two.
    bool volumeOpen = m.volume && m.volumeOpen;
    // A toggle like the other three (on: muted), so at rest it has their circle and, on liquid
    // glass, their shiny edge. Open, the pill is its shape and carries the edge: one ring, not two.
    if (m.volume)
        controls.leading.push_back(Button{ .glyph = volumeGlyph(*m.volume, m.muted), .actionId = act::kMute,
                                           .on = volumeOpen ? std::nullopt : std::optional<bool>(m.muted), .secondary = true });
    if (volumeOpen) {
        controls.leading.push_back(Progress{ .timeline = levelTimeline(m.muted ? 0.0f : std::clamp(*m.volume, 0.0f, 1.0f)), .seekable = true,
                                             .level = true, .actionId = act::kVolume });
        controls.leadingOpen = true;
    } else if (m.canShuffle) {
        controls.leading.push_back(Button{ .glyph = Glyph::Shuffle, .actionId = act::kShuffle, .on = m.shuffle, .secondary = true });
    }
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

} // namespace

Content nothingPlayingContent()
{
    // Stays when left alone too, so the card doesn't close when the music ends (decision 177).
    Content c{ .label = "Nothing playing", .card = CardLayout::Centred, .noIdleTimeout = true };
    c.expandedCenter = { row({ Icon{ Glyph::Music } }, Align::Center),
                         row({ Text{ .value = "Nothing playing", .style = TextStyle::Title, .centred = true } }),
                         row({ Text{ .value = "Music and videos from any app show here", .style = TextStyle::Caption, .centred = true } }) };
    return c;
}

Glyph volumeGlyph(float volume, bool muted)
{
    if (muted || volume <= 0) return Glyph::Muted;
    return volume < 1.0f / 3 ? Glyph::VolumeLow : volume < 2.0f / 3 ? Glyph::VolumeMid : Glyph::Volume;
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
