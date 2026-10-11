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
    c.compactTrailing = { row({ playing ? Item{ Waveform{ true } } : Item{ Icon{ Glyph::Pause } } }) };
    c.minimal = { row({ art }) };
    c.alertLeading = { row({ art }) };
    c.alertCenter = { row({ title }), row({ artist }) };

    c.expandedLeading = { row({ art }) };
    c.expandedCenter = { row({ title }), row({ artist }) };
    if (!m.source.empty()) c.expandedCenter.push_back(row({ Text{ m.source, TextStyle::Caption } }));

    if (m.timeline.duration > Ms{ 0 })
        c.expandedBottom.push_back(row({ TimeLabel{ m.timeline }, Progress{ m.timeline, m.canSeek }, TimeLabel{ m.timeline, true } }));
    Row controls = row({ Button{ Glyph::Previous, act::kPrevious, m.canPrevious },
                         Button{ playing ? Glyph::Pause : Glyph::Play, act::kPlayPause, m.canPlayPause },
                         Button{ Glyph::Next, act::kNext, m.canNext } },
                       Align::Center);
    if (m.showSourceSwitch) controls.items.push_back(Button{ Glyph::Source, act::kSource });
    c.expandedBottom.push_back(controls);
    return c;
}

} // namespace cup
