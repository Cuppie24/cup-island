// Lyrics and the app's volume on media's card (decision 180): LRC text into
// timed lines, what's asked of LRCLIB and which answer is taken, one lookup
// per song, the line being sung as the host picks it, and the volume's bar.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/motion.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/lyrics/lrc.h"
#include "providers/lyrics/lrclib.h"
#include "providers/lyrics/lyrics_lookup.h"
#include "providers/media/media_presenter.h"
#include "providers/media/now_playing.h"

#include <vector>

namespace cup::test {
namespace {

struct LyricsHost : Host {
    Ms clock{ 0 };
    std::vector<Activity> started;
    std::string kept; // what was saved under the lyrics key
    void startActivity(Activity a) override { started.push_back(std::move(a)); }
    void endActivity(const std::string&) override {}
    void postAlert(Alert) override {}
    Ms now() const override { return clock; }
    void publishPanel(Panel) override {}
    void removePanel(const std::string&) override {}
    void wakeAt(std::optional<Ms>) override {}
    void ask(Dialog) override {}
    void reveal(const std::string&, const std::string&, std::size_t) override {}
    std::string load(const std::string& key) const override { return key == MediaPresenter::kLyricsKey ? kept : std::string(); }
    void save(const std::string& key, const std::string& text) override
    {
        if (key == MediaPresenter::kLyricsKey) kept = text;
    }
    Ms wallClock() const override { return clock; }
};

// The service, answering when the test says so.
struct FakeService {
    std::vector<LyricsQuery> asked;
    std::vector<LyricsLookup::Done> waiting;
    LyricsLookup::Fetch fetch()
    {
        return [this](const LyricsQuery& q, LyricsLookup::Done done) {
            asked.push_back(q);
            waiting.push_back(std::move(done));
        };
    }
    void answer(std::optional<std::vector<LrclibRecord>> records)
    {
        auto done = std::move(waiting.front());
        waiting.erase(waiting.begin());
        done(std::move(records));
    }
};

const Placed* find(const Scene& scene, const std::string& key)
{
    for (const auto& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

IslandState expandedWith(const Content& content)
{
    IslandState s;
    s = reduce(s, ActivityStarted{ { .id = "media.nowPlaying", .content = content, .providerId = "media" } }, Ms{ 0 });
    return reduce(s, PillClicked{}, Ms{ 0 });
}

// The controls' left pair: the lyrics button, then the volume's speaker.
std::vector<Item> leftOf(const Content& c)
{
    return c.expandedBottom.empty() ? std::vector<Item>{} : c.expandedBottom.back().leading;
}

bool hasLyricsRow(const Content& c)
{
    return !c.expandedBottom.empty() && c.expandedBottom.front().id == "lyrics";
}

void lrcTests()
{
    auto lines = parseLrc("[ar: Someone]\n[00:12.50] First line\r\n[00:05.00]Before it\n[01:02.345]  Past a minute  \n[00:20.00]\nno time here\n");
    expect(lines.size() == 4 && lines[0] == LyricLine{ Ms{ 5000 }, "Before it" } && lines[1] == LyricLine{ Ms{ 12500 }, "First line" } &&
               lines[2] == LyricLine{ Ms{ 20000 }, "" } && lines[3] == LyricLine{ Ms{ 62345 }, "Past a minute" },
           "LRC: lines by time, trimmed; a tag that isn't a time and a line without one are skipped; a time alone is a break");
    auto twice = parseLrc("[00:10.00][01:10.00]la la\n[00:30.5]half\n[00:40:25]colon\n");
    expect(twice.size() == 4 && twice[0].at == Ms{ 10000 } && twice[1] == LyricLine{ Ms{ 30500 }, "half" } &&
               twice[2] == LyricLine{ Ms{ 40250 }, "colon" } && twice[3] == LyricLine{ Ms{ 70000 }, "la la" },
           "LRC: a line with two times is sung twice; tenths and a ':' before the hundredths are read");
    auto shifted = parseLrc("[offset: +500]\n[00:00.20]a\n[00:10.00]b [not a tag]\n");
    expect(shifted.size() == 2 && shifted[0].at == Ms{ 0 } && shifted[1] == LyricLine{ Ms{ 9500 }, "b [not a tag]" },
           "LRC: an offset brings every line forward, never before the start; brackets in the words stay");
    expect(parseLrc("just words\nand more\n").empty() && parseLrc("").empty() && parseLrc("[xx:yy]bad\n[00:99.123456]bad").empty(),
           "LRC: text with no times gives nothing");
}

void lrclibTests()
{
    LyricsQuery q{ "Don't Stop Me Now", "Queen & Co", Ms{ 209000 } };
    expect(lrclibSearchUrl(q) == "https://lrclib.net/api/search?track_name=Don%27t%20Stop%20Me%20Now&artist_name=Queen%20%26%20Co",
           "LRCLIB: the search by title and artist, percent-encoded");
    expect(lrclibSearchUrl({ "\xD0\x9F", "a", Ms{ 1 } }).find("track_name=%D0%9F&") != std::string::npos, "...its UTF-8 bytes too");
    expect(worthLooking(q) && !worthLooking({ "Title", "", Ms{ 1000 } }) && !worthLooking({ "", "Artist", Ms{ 1000 } }) &&
               !worthLooking({ "Title", "Artist", Ms{ 0 } }),
           "worth asking only with a title, an artist and a length");
    std::vector<LrclibRecord> records{ { Ms{ 215000 }, "[00:01.00]live" }, { Ms{ 209900 }, "" }, { Ms{ 207500 }, "[00:01.00]near" },
                                       { Ms{ 209400 }, "[00:01.00]nearest" } };
    expect(pickSynced(records, q.duration) == "[00:01.00]nearest", "the timed record nearest the track's length; one without timed lyrics never");
    expect(!pickSynced({ records[0], records[1] }, q.duration) && !pickSynced({}, q.duration),
           "none within lyricsDurationTolerance: another cut of the song, not taken");
}

void lookupTests()
{
    FakeService service;
    int changes = 0;
    LyricsLookup lookup(service.fetch(), [&] { ++changes; });
    LyricsQuery song{ "Song", "Artist", Ms{ 100000 } };
    expect(lookup.lyricsFor(song).state == LyricsAnswer::State::Looking && service.asked.size() == 1, "a new song: looked up");
    expect(lookup.lyricsFor(song).state == LyricsAnswer::State::Looking && service.asked.size() == 1, "...once");
    expect(lookup.lyricsFor({ "Video", "", Ms{ 100000 } }).state == LyricsAnswer::State::None && service.asked.size() == 1, "not worth asking: not asked");
    service.answer(std::vector<LrclibRecord>{ { Ms{ 100500 }, "[00:01.00]one\n[00:02.00]two" } });
    LyricsAnswer found = lookup.lyricsFor(song);
    expect(changes == 1 && found.state == LyricsAnswer::State::Found && found.lines && found.lines->size() == 2 && service.asked.size() == 1,
           "its lyrics came: remembered, and the presenter told");
    expect(lookup.lyricsFor({ "Song", "Artist", Ms{ 100400 } }).lines == found.lines, "the same song, its length a moment off: the same lyrics");

    LyricsQuery bare{ "Instrumental", "Artist", Ms{ 50000 } };
    lookup.lyricsFor(bare);
    service.answer(std::vector<LrclibRecord>{});
    expect(lookup.lyricsFor(bare).state == LyricsAnswer::State::None && service.asked.size() == 2, "none to be had: remembered too, not asked again");
    lookup.lyricsFor(song);
    expect(lookup.lyricsFor(bare).state == LyricsAnswer::State::None && service.asked.size() == 2, "...even when it comes up again");

    LyricsQuery offline{ "Offline", "Artist", Ms{ 50000 } };
    lookup.lyricsFor(offline);
    service.answer(std::nullopt);
    expect(lookup.lyricsFor(offline).state == LyricsAnswer::State::None && service.asked.size() == 3, "the request failed: none for now, not retried on every update");
    lookup.lyricsFor(song);
    expect(lookup.lyricsFor(offline).state == LyricsAnswer::State::Looking && service.asked.size() == 4, "...but once more when the song comes up again");

    // Only the songs played lately are kept (lyricsRemembered).
    Tokens saved = tokens(), few = saved;
    few.lyricsRemembered = 2;
    setTokens(few);
    FakeService busy;
    LyricsLookup small(busy.fetch(), [] {});
    for (const char* title : { "A", "B", "C" }) small.lyricsFor({ title, "Artist", Ms{ 1000 } });
    small.lyricsFor({ "A", "Artist", Ms{ 1000 } });
    expect(busy.asked.size() == 4, "a song pushed out of the remembered ones is looked up anew");
    busy.answer(std::nullopt); // A's first lookup, answered late
    expect(small.lyricsFor({ "C", "Artist", Ms{ 1000 } }).state == LyricsAnswer::State::Looking, "...and an answer changes only its own song");
    setTokens(saved);
}

void lineTests()
{
    auto lines = std::make_shared<std::vector<LyricLine>>(std::vector<LyricLine>{ { Ms{ 5000 }, "one" }, { Ms{ 9000 }, "" }, { Ms{ 12000 }, "two" } });
    Timeline playing{ .position = Ms{ 0 }, .duration = Ms{ 60000 }, .anchoredAt = Ms{ 1000 }, .playing = true };
    Lyrics lyrics{ playing, lines };
    expect(lyricAt(lyrics, Ms{ 1000 }) == "♪" && lyricAt(lyrics, Ms{ 6000 }) == "one" && lyricAt(lyrics, Ms{ 10500 }) == "♪" &&
               lyricAt(lyrics, Ms{ 13000 }) == "two" && lyricAt(lyrics, Ms{ 999000 }) == "two",
           "the line shown: the last one begun; a note before the first and in a break");
    expect(lyricAt({ playing, nullptr }, Ms{ 6000 }).empty(), "still being looked up: an empty line");
    // Each line is up lyricLead before it's sung, so its first words can be read in time.
    Ms lead = tokens().lyricLead;
    expect(lead > Ms{ 0 } && lyricAt(lyrics, Ms{ 6000 } - lead) == "one" && lyricAt(lyrics, Ms{ 6000 } - lead - Ms{ 1 }) == "\u266A",
           "a line shows lyricLead before it starts, not sooner");
    expect(nextLyricChange(lyrics, Ms{ 1000 }) == Ms{ 6000 } - lead && nextLyricChange(lyrics, Ms{ 6000 }) == Ms{ 10000 } - lead &&
               !nextLyricChange(lyrics, Ms{ 13000 }),
           "it changes that long before the next line starts; after the last, never");
    Timeline fast = playing, paused = playing;
    fast.rate = 2.0f;
    paused.playing = false;
    expect(nextLyricChange({ fast, lines }, Ms{ 1000 }) == Ms{ 1000 } + (Ms{ 5000 } - lead) / 2 && !nextLyricChange({ paused, lines }, Ms{ 1000 }),
           "sooner at double speed; paused, not at all");

    // On the card: over the bar, across the card, and the scene says when to lay it out again.
    NowPlaying m{ .title = "Song", .artist = "Artist", .timeline = playing, .canSeek = true, .lyrics = lines };
    Content card = nowPlayingContent(m);
    expect(hasLyricsRow(card) && card.expandedBottom.size() == 3 && card.expandedBottom[1].id == "bar" && card.expandedBottom[2].id == "controls",
           "the line being sung: the first row under the cover, over the bar and the controls, each named");
    IslandState s = expandedWith(card);
    Scene before = layout(s, Ms{ 1000 }), during = layout(s, Ms{ 6200 });
    const Placed* line = find(during, "expandedBottom/lyrics/0");
    const Placed* bar = find(during, "expandedBottom/bar/1");
    const Text* text = line ? std::get_if<Text>(&line->item) : nullptr;
    expect(text && text->value == "one" && text->drops && text->centred && text->italic && bar && line->frame.y + line->frame.height <= bar->frame.y,
           "laid out as a centred text that drops in, above the bar");
    const Placed* quietLine = find(before, "expandedBottom/lyrics/0");
    const Text* note = quietLine ? std::get_if<Text>(&quietLine->item) : nullptr;
    expect(note && note->value == kLyricBreak && note->style == TextStyle::CardTitle && !note->italic && text && text->style == TextStyle::Body &&
               quietLine->frame.height == line->frame.height,
           "a break's note sign: the title's size, big enough to see, in a row that keeps its height");
    const Tokens& t = tokens();
    const Placed* cover = find(during, "expandedLeading/0/0");
    expect(card.smallCover && cover && cover->frame.width == t.smallCoverSize && during.size.y == t.expandedSize.y,
           "...in a card still Expanded's height: the smaller cover makes the room");
    // No lyrics: the cover at its full size, in the same card height; between the two it resizes and the rest glides.
    NowPlaying wordless = m;
    wordless.lyrics = nullptr;
    Content plainCard = nowPlayingContent(wordless);
    IslandState plainState = expandedWith(plainCard);
    Scene plain = layout(plainState, Ms{ 6200 });
    const Placed* fullCover = find(plain, "expandedLeading/0/0");
    expect(!plainCard.smallCover && fullCover && fullCover->frame.width == t.expandedArtSize && plain.size.y == t.expandedSize.y,
           "no lyrics: the cover full size, the card the same height");
    expect(coverResized(plain, during) && coverResized(during, plain) && !coverResized(during, before) && !coverResized(plain, plain),
           "the same card around a cover of another size: it resizes and the rest glides; otherwise not");
    Scene compact = layout(reduce(plainState, PillClicked{}, Ms{ 0 }), Ms{ 6200 });
    Scene other = during;
    other.activityId = "another";
    expect(!coverResized(compact, during) && !coverResized(other, plain), "not as the card opens, nor from another card");
    // Its seconds tick over sooner (every 1000 ms); the line's change is among the deadlines.
    expect(line && line->changesAt == Ms{ 10000 } - lead && sceneDeadline(before) && *sceneDeadline(before) <= Ms{ 6000 }, "the next line is a deadline of the scene");
    Content timeless = { .expandedBottom = { row({ Lyrics{ playing, lines } }) } };
    expect(sceneDeadline(layout(expandedWith(timeless), Ms{ 1000 })) == Ms{ 6000 } - lead, "...on its own, the next line's change exactly");
    m.lyrics = nullptr;
    expect(!hasLyricsRow(nowPlayingContent(m)), "no lyrics: no row");
    m.lyricsPending = true;
    expect(hasLyricsRow(nowPlayingContent(m)), "being looked up after a song with some: the row stays, empty");
}

void volumeTests()
{
    const Tokens& t = tokens();
    NowPlaying m{ .title = "Song", .artist = "Artist", .timeline = { .duration = Ms{ 60000 } }, .canSeek = true, .canShuffle = true, .canRepeat = true };
    expect(leftOf(nowPlayingContent(m)).empty() && nowPlayingContent(m).expandedAside.empty(), "volume unknown, no lyrics service: no left pair, no bar");
    m.lyricsOn = true;
    m.volume = 0.25f;
    Content card = nowPlayingContent(m);
    std::vector<Item> left = leftOf(card), right = card.expandedBottom.back().trailing;
    auto button = [](const std::vector<Item>& items, std::size_t i) { return i < items.size() ? std::get_if<Button>(&items[i]) : nullptr; };
    const Button *words = button(left, 0), *speaker = button(left, 1), *shuffle = button(right, 0), *repeat = button(right, 1);
    expect(left.size() == 2 && words && words->glyph == Glyph::Lyrics && words->actionId == media_action::kLyrics && words->on == true && speaker &&
               speaker->glyph == Glyph::Volume && speaker->actionId == media_action::kVolumeOpen && speaker->on == false,
           "the row's left pair: the lyrics toggle, then the speaker that opens the volume");
    expect(right.size() == 2 && shuffle && shuffle->glyph == Glyph::Shuffle && repeat && repeat->glyph == Glyph::Repeat,
           "its right pair: shuffle, then repeat");
    bool alike = words && speaker && shuffle && repeat;
    for (const Button* b : { words, speaker, shuffle, repeat }) alike = alike && b->secondary && !b->compact && !b->large && b->on.has_value();
    expect(alike, "all four the same quiet toggles");
    // Mirrored about the card's middle, with ▶ on it.
    Scene closed = layout(expandedWith(card), Ms{ 0 });
    auto at = [&](const Scene& s, const char* key) { return find(s, std::string("expandedBottom/controls/") + key); };
    const Placed *play = at(closed, "1"), *pShuffle = at(closed, "3"), *pRepeat = at(closed, "4"), *pWords = at(closed, "5"), *pSpeaker = at(closed, "6");
    float mid = closed.size.x / 2;
    auto centre = [](const Placed* p) { return p->frame.x + p->frame.width / 2; };
    expect(play && pShuffle && pRepeat && pWords && pSpeaker && std::abs(centre(play) - mid) < 0.01f &&
               std::abs((mid - centre(pWords)) - (centre(pRepeat) - mid)) < 0.01f && std::abs((mid - centre(pSpeaker)) - (centre(pShuffle) - mid)) < 0.01f &&
               pWords->frame.width == pRepeat->frame.width && pSpeaker->frame.width == pShuffle->frame.width && pWords->frame.x == t.expandedPadding,
           "the pairs mirror each other about the card's middle, at its edges, the same size");
    expect(card.expandedAside.empty() && !find(closed, "expandedAside/0/0"), "the volume's bar isn't shown until its speaker is clicked");

    m.volumeOpen = true;
    Content open = nowPlayingContent(m);
    auto* level = open.expandedAside.size() == 1 && open.expandedAside[0].items.size() == 1 ? std::get_if<Progress>(&open.expandedAside[0].items[0]) : nullptr;
    expect(level && level->level && level->seekable && level->actionId == media_action::kVolume && level->timeline.fractionAt(Ms{ 0 }) == 0.25f &&
               std::get<Button>(leftOf(open)[1]).on == true,
           "clicked: its bar beside the song's name, at the app's level, and the speaker lit");
    Scene scene = layout(expandedWith(open), Ms{ 0 });
    const Placed* title = find(scene, "expandedCenter/0/0");
    const Placed* bar = find(scene, "expandedAside/0/0");
    const Placed* cover = find(scene, "expandedLeading/0/0");
    auto middle = [](const Placed* p) { return p->frame.y + p->frame.height / 2; };
    expect(title && bar && cover && bar->frame.width == t.levelBarWidth && title->frame.x + title->frame.width + t.slotGap <= bar->frame.x + 0.01f &&
               std::abs(bar->frame.x + bar->frame.width - (scene.size.x - t.expandedPadding)) < 0.01f && std::abs(middle(bar) - middle(cover)) < 0.01f,
           "the bar at the card's right edge, levelBarWidth wide, in the middle of the cover's height, the name taking the rest");
    if (bar) {
        auto hit = hitTest(scene, { bar->frame.x + bar->frame.width * 0.75f, bar->frame.y + bar->frame.height / 2 });
        expect(hit && hit->action.actionId == media_action::kVolume && std::abs(hit->action.value - 0.75f) < 0.01f && hit->action.providerId == "media",
               "a click on it sends the volume asked for, not a seek");
    }
    const Placed* seek = find(scene, "expandedBottom/bar/1");
    auto seeking = seek ? hitTest(scene, { seek->frame.x + 1, seek->frame.y + seek->frame.height / 2 }) : std::nullopt;
    expect(seeking && seeking->action.actionId == media_action::kSeek, "the song's own bar still seeks");
    m.muted = true;
    Content silent = nowPlayingContent(m);
    expect(std::get<Button>(leftOf(silent)[1]).glyph == Glyph::Muted && std::get<Progress>(silent.expandedAside[0].items[0]).timeline.fractionAt(Ms{ 0 }) == 0.0f,
           "muted: the crossed speaker, the bar empty");
    m.muted = false;
    m.volume = 0.0f;
    expect(std::get<Button>(leftOf(nowPlayingContent(m))[1]).glyph == Glyph::Muted, "...and at no volume at all");
    expect(levelTimeline(2.0f).fractionAt(Ms{ 5 }) == 1.0f && levelTimeline(-1.0f).fractionAt(Ms{ 5 }) == 0.0f && !levelTimeline(0.5f).playing,
           "a level's timeline: clamped, and standing still");
}

void presenterTests()
{
    LyricsHost host;
    FakeService service;
    MediaPresenter presenter(host, service.fetch());
    auto song = [](const char* title, const char* app = "Spotify.exe") {
        return MediaSnapshot{ app, { .title = title, .artist = "Artist", .timeline = { .duration = Ms{ 100000 }, .playing = true } } };
    };
    presenter.start();
    presenter.show(song("One"));
    expect(host.started.size() == 1 && !hasLyricsRow(host.started.back().content) && service.asked.size() == 1 && service.asked[0].title == "One",
           "a song shows at once, its lyrics asked for; no row before any song had lyrics");
    service.answer(std::vector<LrclibRecord>{ { Ms{ 100000 }, "[00:01.00]la" } });
    expect(host.started.size() == 2 && hasLyricsRow(host.started.back().content), "its lyrics came: shown again with the line");
    presenter.show(song("One"));
    expect(host.started.size() == 2, "the same read again: nothing new");

    presenter.show(song("Two"));
    expect(host.started.size() == 3 && hasLyricsRow(host.started.back().content) &&
               !std::get<Lyrics>(host.started.back().content.expandedBottom.front().items[0]).lines,
           "the next song, looked up: the row stays, empty, so the bar doesn't jump");
    service.answer(std::vector<LrclibRecord>{});
    expect(host.started.size() == 4 && !hasLyricsRow(host.started.back().content), "it has none: the row goes");
    presenter.show(song("Three"));
    expect(!hasLyricsRow(host.started.back().content), "...and stays away while the song after it is looked up");
    presenter.show(song("One"));
    expect(hasLyricsRow(host.started.back().content) && service.asked.size() == 3, "back to the first: its lyrics at once, nothing asked");

    // The app's volume: shown for the session it was read for.
    std::size_t shown = host.started.size();
    presenter.showVolume("Spotify.exe", 0.5f, false);
    expect(host.started.size() == shown + 1 && leftOf(host.started.back().content).size() == 2, "the app's volume read: its control appears");
    presenter.showVolume("Spotify.exe", 0.5f, false);
    expect(host.started.size() == shown + 1, "the same again: nothing new");
    presenter.showPlayback("Spotify.exe", NowPlaying{ .timeline = { .duration = Ms{ 100000 } } });
    expect(leftOf(host.started.back().content).size() == 2 && hasLyricsRow(host.started.back().content),
           "a pause keeps the volume and the lyrics, as it keeps the title");
    presenter.show(song("One", "chrome.exe"));
    expect(leftOf(host.started.back().content).size() == 1, "another app: its volume isn't known yet");
    presenter.showVolume("Spotify.exe", 0.2f, true);
    expect(leftOf(host.started.back().content).size() == 1, "a late answer about the app before: not shown on this one");
    presenter.showVolume("chrome.exe", std::nullopt, false);
    expect(leftOf(host.started.back().content).size() == 1, "an app with no audio session: no control");

    // The speaker opens the volume's bar and puts it away; the card closing puts it away too.
    presenter.showVolume("chrome.exe", 0.4f, false);
    expect(host.started.back().content.expandedAside.empty(), "a volume known: its bar still closed");
    presenter.toggleVolume();
    expect(host.started.back().content.expandedAside.size() == 1, "the speaker clicked: the bar shows");
    presenter.show(song("Six", "chrome.exe"));
    expect(host.started.back().content.expandedAside.size() == 1, "...and stays through the next song");
    presenter.closeVolume();
    std::size_t closedAt = host.started.size();
    presenter.closeVolume();
    expect(host.started.back().content.expandedAside.empty() && host.started.size() == closedAt, "the card closed: put away, once");

    // The lyrics button: off, the line goes, the cover is full size again and nothing more is asked; remembered.
    presenter.show(song("One"));
    std::size_t asked = service.asked.size();
    expect(hasLyricsRow(host.started.back().content) && host.started.back().content.smallCover, "lyrics on, a song with some: the line, a small cover");
    presenter.toggleLyrics();
    const Content& quiet = host.started.back().content;
    expect(!hasLyricsRow(quiet) && !quiet.smallCover && std::get<Button>(leftOf(quiet)[0]).on == false && host.kept == "off",
           "turned off: no line, the cover full size, the button off, and it's kept");
    presenter.show(song("Four"));
    expect(service.asked.size() == asked && !hasLyricsRow(host.started.back().content), "off: a new song is not looked up");
    {
        FakeService later;
        MediaPresenter restarted(host, later.fetch());
        restarted.start();
        restarted.show(song("Five"));
        expect(later.asked.empty() && std::get<Button>(leftOf(host.started.back().content)[0]).on == false, "still off after a restart");
    }
    presenter.toggleLyrics();
    expect(host.kept == "on" && service.asked.size() == asked + 1 && service.asked.back().title == "Four", "turned on again: the song playing is looked up");
    presenter.show(song("One"));
    expect(hasLyricsRow(host.started.back().content), "...and a song already known has its line back at once");

    // Without a lyrics service (the default): never a row, nothing asked.
    LyricsHost plainHost;
    MediaPresenter plain(plainHost);
    plain.show(song("One"));
    expect(plainHost.started.size() == 1 && !hasLyricsRow(plainHost.started.back().content) &&
               plainHost.started.back().content.expandedAside.empty(),
           "no lyrics service: the card as it was, with no lyrics button");
}

} // namespace

void runLyricsTests()
{
    lrcTests();
    lrclibTests();
    lookupTests();
    lineTests();
    volumeTests();
    presenterTests();
}

} // namespace cup::test
