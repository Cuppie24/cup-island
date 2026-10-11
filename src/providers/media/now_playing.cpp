#include "providers/media/now_playing.h"

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
    c.compactTrailing = { row({ Waveform{ playing } }, Align::Center) }; // paused: the bars rest as dots
    c.minimal = { row({ art }) };
    c.alertLeading = { row({ art }) };
    c.alertCenter = { row({ title }), row({ artist }) };

    c.expandedLeading = { row({ art }) };
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
