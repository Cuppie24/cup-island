// Lyrics and the app's volume on media's card (decision 180): LRC text into
// timed lines, what's asked of LRCLIB and which answer is taken, one lookup
// per song, the line being sung as the host picks it, and the volume's bar.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/motion.h"
#include "core/tokens.h"
#include "core/tokens_json.h"
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
    std::vector<std::string> revealed, followed, centred; // "<list>:<row>"
    void centre(const std::string&, const std::string& list, std::size_t row) override { centred.push_back(list + ":" + std::to_string(row)); }
    void follow(const std::string&, const std::string& list, std::size_t row) override { followed.push_back(list + ":" + std::to_string(row)); }
    std::string kept; // what was saved under the lyrics key
    void startActivity(Activity a) override { started.push_back(std::move(a)); }
    void endActivity(const std::string&) override {}
    void postAlert(Alert) override {}
    Ms now() const override { return clock; }
    void publishPanel(Panel) override {}
    void removePanel(const std::string&) override {}
    std::optional<Ms> wake;
    void wakeAt(std::optional<Ms> at) override { wake = at; }
    void ask(Dialog) override {}
    void reveal(const std::string&, const std::string& list, std::size_t row) override { revealed.push_back(list + ":" + std::to_string(row)); }
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

// The button with this action at an end of the controls' row (the speaker, shuffle, repeat, lyrics), if it's there.
const Button* controlOf(const Content& c, const char* action)
{
    if (c.expandedBottom.empty()) return nullptr;
    const Row& controls = c.expandedBottom.back();
    for (const auto* edge : { &controls.leading, &controls.trailing })
        for (const Item& item : *edge)
            if (auto* button = std::get_if<Button>(&item); button && button->actionId == action) return button;
    return nullptr;
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

// The lyrics' page (the line being sung clicked) and the line in the pill.
void pageAndPillTests()
{
    const Tokens& t = tokens();
    Ms lead = t.lyricLead;
    auto lines = std::make_shared<std::vector<LyricLine>>(std::vector<LyricLine>{
        { Ms{ 10000 }, "one" }, { Ms{ 20000 }, "two" }, { Ms{ 30000 }, "three" }, { Ms{ 40000 }, "" }, { Ms{ 50000 }, "five" } });
    Timeline playing{ .position = Ms{ 0 }, .duration = Ms{ 90000 }, .anchoredAt = Ms{ 0 }, .playing = true };
    NowPlaying m{ .title = "Song", .artist = "Artist", .timeline = playing, .canSeek = true, .lyrics = lines, .lyricsOn = true };

    // The card's line is clicked like a button; an empty row (still looked up) isn't.
    Scene card = layout(expandedWith(nowPlayingContent(m)), Ms{ 31000 });
    const Placed* line = find(card, "expandedBottom/lyrics/0");
    auto click = line ? hitTest(card, { line->frame.x + line->frame.width / 2, line->frame.y + line->frame.height / 2 }) : std::nullopt;
    expect(click && click->action.actionId == media_action::kLyricsPage && click->action.providerId == "media", "a click on the line being sung asks for its page");
    const Text* cardLine = line ? std::get_if<Text>(&line->item) : nullptr;
    expect(cardLine && cardLine->italic && cardLine->drops && cardLine->rises, "on the card it's italic, and the next line comes up from below");
    NowPlaying looking = m;
    looking.lyrics = nullptr;
    looking.lyricsPending = true;
    Scene waiting = layout(expandedWith(nowPlayingContent(looking)), Ms{ 31000 });
    const Placed* empty = find(waiting, "expandedBottom/lyrics/0");
    expect(empty && !hitTest(waiting, { empty->frame.x + empty->frame.width / 2, empty->frame.y + empty->frame.height / 2 }),
           "no lyrics yet: nothing to open");

    // The page: a back arrow, the song's name and a sync button, then every line in a list the host scrolls.
    m.lyricsPage = true;
    m.lyricsLine = 2;
    Content page = nowPlayingContent(m);
    expect(page.card == CardLayout::Centred && page.backAction == media_action::kLyricsBack && page.expandedCenter.size() == 1 + lines->size() &&
               page.expandedBottom.empty() && page.expandedLeading.empty() && !page.smallCover && page.pageAtTop && !page.sizedLike.empty(),
           "the page: one stack of rows from the card's top, Esc goes back");
    expect(page.label == nowPlayingContent(NowPlaying{ .title = "Song", .artist = "Artist" }).label && !page.compactLeading.empty() && page.noIdleTimeout,
           "...the pill and the rest off the card as they were");
    const Row& heading = page.expandedCenter[0];
    auto* back = heading.leading.size() == 1 ? std::get_if<Button>(&heading.leading[0]) : nullptr;
    auto* sync = heading.trailing.size() == 1 ? std::get_if<Button>(&heading.trailing[0]) : nullptr;
    expect(heading.items.empty(), "no song's name over the words: only the two buttons");
    expect(back && back->glyph == Glyph::ChevronLeft && back->actionId == media_action::kLyricsBack && sync && sync->glyph == Glyph::Sync &&
               sync->actionId == media_action::kLyricsSync && heading.list.empty(),
           "its back arrow at the first row's start, its sync button at the end; the heading doesn't scroll");
    auto textOf = [&](std::size_t line) { return std::get_if<Text>(&page.expandedCenter[1 + line].items[0]); };
    bool listed = true;
    for (std::size_t i = 0; i < lines->size(); ++i) listed = listed && page.expandedCenter[1 + i].list == media_action::kLyricsList && textOf(i);
    expect(listed && page.expandedCenter[1].visibleRows < 0 && textOf(0)->value == "one" && textOf(2)->value == "three" && textOf(4)->value == "five",
           "every line of the song a row of one list, which fills the rest of the card");
    expect(textOf(2)->style == TextStyle::Title && textOf(1)->style == TextStyle::Body && textOf(4)->style == TextStyle::Body && textOf(2)->centred &&
               textOf(2)->wrap && !textOf(2)->italic,
           "the one being sung bigger and bright, the rest quieter; long ones wrap");
    expect(textOf(3)->value == kLyricBreak && textOf(3)->style == TextStyle::Body && !textOf(3)->wrap, "a line with no words keeps its row: a note in it");
    m.lyricsLine = 3;
    expect(std::get<Text>(nowPlayingContent(m).expandedCenter[4].items[0]).style == TextStyle::Title, "...lit like any other while it's the one being sung");
    m.lyricsLine = 2;
    // Laid out: the card's height, the list a window under the heading that scrolls.
    Scene shown = layout(expandedWith(page), Ms{ 31000 });
    const Placed *first = find(shown, "expandedCenter/line0/0"), *header = find(shown, "expandedCenter/heading/0"); // the sync button: the heading has no text
    expect(shown.size.y == t.expandedSize.y && first && header && first->list && first->list->id == media_action::kLyricsList &&
               first->list->viewport.y >= header->frame.y + header->frame.height && first->list->max == 0,
           "in a card of Expanded's height, the list under the heading; a short song fits and doesn't scroll");
    // A real song is longer than the card: the list fills what's left of it and scrolls.
    auto many = std::make_shared<std::vector<LyricLine>>();
    for (int i = 0; i < 40; ++i) many->push_back({ Ms{ 2000 * (i + 1) }, "line " + std::to_string(i) });
    NowPlaying longSong = m;
    longSong.lyrics = many;
    Scene tall = layout(expandedWith(nowPlayingContent(longSong)), Ms{ 0 });
    const Placed* top = find(tall, "expandedCenter/line0/0");
    expect(tall.size.y == t.expandedSize.y && top && top->list && top->list->max > 0 &&
               std::abs(top->list->viewport.y + top->list->viewport.height - (tall.size.y - t.expandedPadding)) < 1.5f,
           "a long song: still Expanded's height, the list down to the card's padding, with more to scroll to");
    auto syncHit = hitTest(shown, { shown.size.x - t.expandedPadding - 8, header->frame.y + header->frame.height / 2 });
    expect(syncHit && syncHit->action.actionId == media_action::kLyricsSync, "the sync button takes its click");
    // What's kept in the page's middle: the line being sung.
    expect(lyricsPageRow(m) == 2, "kept in the middle: the one being sung");
    m.lyricsLine = -1;
    expect(lyricsPageRow(m) == 0 && std::get<Text>(nowPlayingContent(m).expandedCenter[1].items[0]).style == TextStyle::Body, "before the first line: the top, nothing lit");
    m.lyricsLine = 4;
    expect(lyricsPageRow(m) == 4, "at the last line: the last row");
    // Centred (Host::centre): the host scrolls the list so that row's middle is the window's; at the song's ends as near as it gets.
    {
        IslandState paged = expandedWith(nowPlayingContent(longSong));
        auto rowAt = [&](IslandState state, std::size_t row) {
            state = reduce(state, ListRevealed{ "media.nowPlaying", media_action::kLyricsList, row, false, true }, Ms{ 0 });
            Scene scene = layout(state, Ms{ 0 });
            const Placed* p = find(scene, "expandedCenter/line" + std::to_string(row) + "/0");
            Rect window = p->list->viewport;
            return std::pair<float, float>{ p->frame.y + p->frame.height / 2 - (window.y + window.height / 2), p->list->offset };
        };
        auto [middle, scrolled] = rowAt(paged, 20);
        expect(std::abs(middle) < 1.0f && scrolled > 0, "a line in the song's middle: in the window's middle");
        expect(rowAt(paged, 0).second == 0 && rowAt(paged, 0).first < 0, "the first line: the list at its top, the line above the middle");
        expect(rowAt(paged, 39).first > 0, "the last: the list at its end, the line below the middle");
        IslandState edge = reduce(paged, ListRevealed{ "media.nowPlaying", media_action::kLyricsList, 20 }, Ms{ 0 });
        const Scene edged = layout(edge, Ms{ 0 });
        const Placed* shownRow = find(edged, "expandedCenter/line20/0");
        expect(shownRow && shownRow->frame.y + shownRow->frame.height / 2 > shownRow->list->viewport.y + shownRow->list->viewport.height / 2 + 20,
               "a plain reveal only brings it in at the window's edge");
    }
    m.lyrics = nullptr;
    expect(nowPlayingContent(m).card == CardLayout::Standard, "a song with no lyrics: the card, not an empty page");

    // The pill: the line being sung between its ends, upright, in a wider pill; paused or without lyrics, the song's name in the usual one.
    m.lyrics = lines;
    m.lyricsPage = false;
    Content sungContent = nowPlayingContent(m);
    IslandState pill;
    pill = reduce(pill, ActivityStarted{ { .id = "media.nowPlaying", .relevance = 0.9f, .content = sungContent, .providerId = "media" } }, Ms{ 0 });
    Scene compact = layout(pill, Ms{ 31000 });
    const Placed* centre = find(compact, "compactCenter/0/0");
    const Text* words = centre ? std::get_if<Text>(&centre->item) : nullptr;
    expect(sungContent.widePill && compact.mode == Mode::Compact && compact.size.x == t.widePillWidth && compact.size.y == t.compactSize.y && words &&
               words->value == "three" && words->drops && !words->italic && centre->changesAt == Ms{ 40000 } - lead,
           "the pill: wider, the line being sung in it, upright, changing with the song");
    expect(std::abs(compact.offset.x + compact.size.x / 2 - t.canvas.x / 2) < 0.01f, "...still centred where the pill rests");
    // A line too long to fit is scrolled through by the time the next one comes: the text says by when.
    expect(words && words->scrollUntil == centre->changesAt && cardLine && cardLine->scrollUntil == Ms{ 40000 } - lead,
           "a line carries the moment it changes: too long, it's scrolled through by then");
    Tokens scaled = scaleTokens(Tokens{}, 2.0f);
    expect(scaled.lyricScrollHoldStart == Tokens{}.lyricScrollHoldStart && scaled.lyricScrollHoldEnd == Tokens{}.lyricScrollHoldEnd &&
               std::abs(scaled.widePillWidth / Tokens{}.widePillWidth - scaled.compactSize.x / Tokens{}.compactSize.x) < 0.001f,
           "the scroll's holds are shares of time, not lengths: display scale leaves them; the wide pill scales as the pill does");
    NowPlaying paused = m;
    paused.timeline.playing = false;
    Content pausedContent = nowPlayingContent(paused);
    NowPlaying wordless = m;
    wordless.lyrics = nullptr;
    IslandState plainPill;
    plainPill = reduce(plainPill, ActivityStarted{ { .id = "media.nowPlaying", .content = nowPlayingContent(wordless), .providerId = "media" } }, Ms{ 0 });
    Scene plain = layout(plainPill, Ms{ 31000 });
    const Placed* name = find(plain, "compactCenter/0/0");
    expect(!pausedContent.widePill && !nowPlayingContent(wordless).widePill && plain.size.x == t.compactSize.x && name &&
               std::get<Text>(name->item).marquee,
           "paused, or no lyrics: the usual pill with the song's name");
    // Split: the bubble sits beside the wider pill, not over its end.
    IslandState two = reduce(pill, ActivityStarted{ { .id = "timer", .content = { .minimal = { row({ Icon{ Glyph::Timer } }) } }, .providerId = "t" } }, Ms{ 0 });
    Scene split = layout(two, Ms{ 31000 });
    expect(split.mode == Mode::Split && split.bubble && split.size.x == t.widePillWidth && split.bubble->frame.x >= split.offset.x + split.size.x,
           "with a second activity: the bubble clear of the wider pill");
}

void volumeTests()
{
    const Tokens& t = tokens();
    NowPlaying m{ .title = "Song", .artist = "Artist", .timeline = { .duration = Ms{ 60000 } }, .canSeek = true, .canShuffle = true, .canRepeat = true };
    Content bare = nowPlayingContent(m);
    expect(!controlOf(bare, media_action::kMute) && !controlOf(bare, media_action::kLyrics),
           "volume unknown, no lyrics service: no speaker, no lyrics button, no bar");
    m.lyricsOn = true;
    m.volume = 0.25f;
    Content card = nowPlayingContent(m);
    const std::vector<Item> &left = card.expandedBottom.back().leading, &right = card.expandedBottom.back().trailing;
    auto button = [](const std::vector<Item>& items, std::size_t i) { return i < items.size() ? std::get_if<Button>(&items[i]) : nullptr; };
    const Button *speaker = button(left, 0), *shuffle = button(left, 1), *repeat = button(right, 0), *words = button(right, 1);
    expect(left.size() == 2 && speaker && speaker->glyph == Glyph::VolumeLow && speaker->actionId == media_action::kMute && speaker->on == false &&
               shuffle && shuffle->glyph == Glyph::Shuffle,
           "the row's left pair: the speaker, which mutes, then shuffle");
    expect(right.size() == 2 && repeat && repeat->glyph == Glyph::Repeat && words && words->glyph == Glyph::Lyrics &&
               words->actionId == media_action::kLyrics && words->on == true && words->enabled,
           "its right pair: repeat, then the lyrics toggle");
    bool alike = words && speaker && shuffle && repeat;
    for (const Button* b : { words, speaker, shuffle, repeat }) alike = alike && b->secondary && !b->compact && !b->large && b->on.has_value();
    expect(alike, "all four the same quiet toggles: the same circle and, on liquid glass, the same shiny edge at rest");
    // Mirrored about the card's middle, with ▶ on it.
    Scene closed = layout(expandedWith(card), Ms{ 0 });
    auto at = [&](const Scene& s, const char* key) { return find(s, std::string("expandedBottom/controls/") + key); };
    const Placed *play = at(closed, "1"), *pRepeat = at(closed, "3"), *pWords = at(closed, "4"), *pSpeaker = at(closed, "5"), *pShuffle = at(closed, "6");
    float mid = closed.size.x / 2;
    auto centre = [](const Placed* p) { return p->frame.x + p->frame.width / 2; };
    expect(play && pShuffle && pRepeat && pWords && pSpeaker && std::abs(centre(play) - mid) < 0.01f &&
               std::abs((mid - centre(pSpeaker)) - (centre(pWords) - mid)) < 0.01f && std::abs((mid - centre(pShuffle)) - (centre(pRepeat) - mid)) < 0.01f &&
               pWords->frame.width == pSpeaker->frame.width && pRepeat->frame.width == pShuffle->frame.width && pSpeaker->frame.x == t.expandedPadding,
           "the pairs mirror each other about the card's middle: the speaker and lyrics at its edges, shuffle and repeat inside them");
    // A song with no lyrics: the button stays, disabled, so the row's end doesn't change; shuffle and repeat go with the app's support.
    NowPlaying wordless = m;
    wordless.lyricsMissing = true;
    wordless.canShuffle = wordless.canRepeat = false;
    Content quietCard = nowPlayingContent(wordless);
    const Button* none = controlOf(quietCard, media_action::kLyrics);
    expect(none && !none->enabled && none->on == false && !controlOf(quietCard, media_action::kShuffle) && !controlOf(quietCard, media_action::kRepeat) &&
               controlOf(quietCard, media_action::kMute),
           "no lyrics for this song: the lyrics button disabled, not gone; shuffle and repeat gone where the app has none");
    Scene quietScene = layout(expandedWith(quietCard), Ms{ 0 });
    const Placed* lone = find(quietScene, "expandedBottom/controls/3"); // the lyrics button, alone at the right
    expect(lone && pWords && lone->frame == pWords->frame && !hitTest(quietScene, { centre(lone), lone->frame.y + lone->frame.height / 2 }),
           "...where it always is, and a click on it does nothing");
    if (lone) {
        Vec2 onIt{ centre(lone), lone->frame.y + lone->frame.height / 2 };
        expect(onDisabledControl(quietScene, onIt) && !onDisabledControl(closed, onIt) && !onDisabledControl(quietScene, { quietScene.size.x / 2, 60 }),
               "...not even close the card: it's a click on a disabled control, not on the card");
    }
    expect(!card.expandedBottom.back().leadingOpen && !find(closed, "expandedBottom/controls/open"), "the volume's bar isn't shown until the pointer is on its speaker");

    // With the pointer on the speaker it opens to its right into the bar, over shuffle's place, on one pill.
    m.volumeOpen = true;
    Content open = nowPlayingContent(m);
    const Row& opened = open.expandedBottom.back();
    auto* level = opened.leading.size() == 2 ? std::get_if<Progress>(&opened.leading[1]) : nullptr;
    expect(opened.leadingOpen && level && level->level && level->seekable && !level->wavy && level->actionId == media_action::kVolume &&
               level->timeline.fractionAt(Ms{ 0 }) == 0.25f && button(opened.leading, 0) && button(opened.leading, 0)->actionId == media_action::kMute &&
               !button(opened.leading, 0)->on && !controlOf(open, media_action::kShuffle) && controlOf(open, media_action::kRepeat),
           "open: the speaker, then the volume's bar at the app's level where shuffle was; the right pair as it was");
    Scene scene = layout(expandedWith(open), Ms{ 0 });
    const Placed *opener = at(scene, "5"), *bar = at(scene, "6"), *pill = at(scene, "open");
    expect(opener && bar && pill && opener->frame == pSpeaker->frame && std::holds_alternative<Progress>(bar->item) && bar->frame.width == t.levelBarWidth &&
               bar->frame.x == opener->frame.x + opener->frame.width + t.itemSpacing,
           "the speaker where it was, the bar levelBarWidth wide right beside it");
    auto* fill = pill ? std::get_if<GroupFill>(&pill->item) : nullptr;
    expect(fill && fill->unrolls && pill->frame.x == opener->frame.x && pill->frame.y == opener->frame.y && pill->frame.height == opener->frame.height &&
               std::abs(pill->frame.x + pill->frame.width - (bar->frame.x + bar->frame.width + t.openButtonPadding)) < 0.01f &&
               pill->cornerRadius == pill->frame.height / 2,
           "one pill under the two, the speaker's height, from its start to a little past the bar, growing out of the speaker");
    expect(at(scene, "0") && at(closed, "0") && at(scene, "0")->frame == at(closed, "0")->frame && at(scene, "1")->frame == at(closed, "1")->frame &&
               pill->frame.x + pill->frame.width < at(scene, "0")->frame.x && pill->frame.x + pill->frame.width > pShuffle->frame.x + pShuffle->frame.width &&
               scene.size.y == closed.size.y,
           "it covers shuffle's place and stops short of previous; the transport doesn't move");
    if (bar && opener) {
        auto hit = hitTest(scene, { bar->frame.x + bar->frame.width * 0.75f, bar->frame.y + bar->frame.height / 2 });
        expect(hit && hit->action.actionId == media_action::kVolume && std::abs(hit->action.value - 0.75f) < 0.01f && hit->action.providerId == "media",
               "a click on the bar sends the volume asked for");
        auto mute = hitTest(scene, { centre(opener), opener->frame.y + opener->frame.height / 2 });
        expect(mute && mute->action.actionId == media_action::kMute, "a click on the speaker mutes");
        // Between the two and just past the bar the pointer is still on one of them: the pill doesn't shut under it.
        auto between = hitTest(scene, { opener->frame.x + opener->frame.width + t.itemSpacing / 2 + 1, opener->frame.y + opener->frame.height / 2 });
        auto past = hitTest(scene, { bar->frame.x + bar->frame.width + t.openButtonPadding - 1, bar->frame.y + bar->frame.height / 2 });
        expect(between && past && past->action.actionId == media_action::kVolume, "no gap in the pill where the pointer is on neither");
    }
    const Placed* seek = find(scene, "expandedBottom/bar/1");
    auto seeking = seek ? hitTest(scene, { seek->frame.x + 1, seek->frame.y + seek->frame.height / 2 }) : std::nullopt;
    expect(seeking && seeking->action.actionId == media_action::kSeek, "the song's own bar is there and seeks all the while");
    m.muted = true;
    Content silent = nowPlayingContent(m);
    m.volumeOpen = false;
    expect(controlOf(nowPlayingContent(m), media_action::kMute)->on == true, "muted, at rest: the toggle is on");
    m.volumeOpen = true;
    expect(controlOf(silent, media_action::kMute)->glyph == Glyph::Muted &&
               std::get<Progress>(silent.expandedBottom.back().leading[1]).timeline.fractionAt(Ms{ 0 }) == 0.0f,
           "muted: the crossed speaker, the bar empty");
    m.muted = false;
    m.volume = 0.0f;
    expect(controlOf(nowPlayingContent(m), media_action::kMute)->glyph == Glyph::Muted, "...and at no volume at all");
    // The speaker says how loud: more waves by thirds.
    expect(volumeGlyph(0.0f, false) == Glyph::Muted && volumeGlyph(0.8f, true) == Glyph::Muted && volumeGlyph(0.1f, false) == Glyph::VolumeLow &&
               volumeGlyph(0.5f, false) == Glyph::VolumeMid && volumeGlyph(0.9f, false) == Glyph::Volume && volumeGlyph(1.0f, false) == Glyph::Volume,
           "the speaker's waves follow the level: none muted or at nothing, then one, two, three");
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
    expect(host.started[2].content.expandedBottom.size() && controlOf(host.started[2].content, media_action::kLyrics)->enabled &&
               !controlOf(host.started.back().content, media_action::kLyrics)->enabled,
           "...and the lyrics button is disabled for it; while it was looked up, it wasn't");
    presenter.show(song("Three"));
    expect(!hasLyricsRow(host.started.back().content), "...and stays away while the song after it is looked up");
    presenter.show(song("One"));
    expect(hasLyricsRow(host.started.back().content) && service.asked.size() == 3, "back to the first: its lyrics at once, nothing asked");

    // The app's volume: shown for the session it was read for.
    std::size_t shown = host.started.size();
    presenter.showVolume("Spotify.exe", 0.5f, false);
    expect(host.started.size() == shown + 1 && controlOf(host.started.back().content, media_action::kMute), "the app's volume read: its control appears");
    presenter.showVolume("Spotify.exe", 0.5f, false);
    expect(host.started.size() == shown + 1, "the same again: nothing new");
    presenter.showPlayback("Spotify.exe", NowPlaying{ .timeline = { .duration = Ms{ 100000 } } });
    expect(controlOf(host.started.back().content, media_action::kMute) && hasLyricsRow(host.started.back().content),
           "a pause keeps the volume and the lyrics, as it keeps the title");
    presenter.show(song("One", "chrome.exe"));
    expect(!controlOf(host.started.back().content, media_action::kMute), "another app: its volume isn't known yet");
    presenter.showVolume("Spotify.exe", 0.2f, true);
    expect(!controlOf(host.started.back().content, media_action::kMute), "a late answer about the app before: not shown on this one");
    presenter.showVolume("chrome.exe", std::nullopt, false);
    expect(!controlOf(host.started.back().content, media_action::kMute), "an app with no audio session: no control");

    // The pointer on the speaker opens the volume's bar; it stays open while the pointer is on either, and shuts when it's on neither.
    auto volumeShown = [&] { return host.started.back().content.expandedBottom.back().leadingOpen; };
    presenter.showVolume("chrome.exe", 0.4f, false);
    expect(!volumeShown(), "a volume known: its bar still shut");
    host.clock = Ms{ 20000 };
    presenter.hoverVolume(false, true);
    expect(!volumeShown() && host.wake == Ms{ 20000 } + tokens().volumeOpenDelay, "the pointer on the speaker: not yet, it may only be passing");
    presenter.hoverVolume(false, false);
    expect(!volumeShown() && !host.wake, "gone before the delay: nothing opens");
    presenter.hoverVolume(false, true);
    host.clock = Ms{ 20000 } + tokens().volumeOpenDelay;
    presenter.onWake();
    expect(volumeShown() && !host.wake, "still there after the delay: it opens");
    std::size_t opened = host.started.size();
    presenter.hoverVolume(true, true);  // onto the bar first...
    presenter.hoverVolume(false, false); // ...then off the speaker, as the app tells it
    expect(volumeShown() && host.started.size() == opened, "from the speaker onto the bar: open throughout, nothing republished");
    presenter.show(song("Six", "chrome.exe"));
    expect(volumeShown(), "...and through the next song");
    presenter.hoverVolume(true, false);
    expect(!volumeShown(), "off both: it shuts");
    presenter.hoverVolume(false, true);
    host.clock += tokens().volumeOpenDelay;
    presenter.onWake();
    presenter.closeVolume();
    std::size_t closedAt = host.started.size();
    presenter.closeVolume();
    expect(!volumeShown() && host.started.size() == closedAt, "the card closing shuts it whatever the pointer was on, once");
    presenter.hoverVolume(true, false);
    expect(!volumeShown() && host.started.size() == closedAt, "a late 'left' after that changes nothing");

    // The line being sung clicked: the lyrics' page, until its back arrow; a song without lyrics shows the card.
    host.clock = Ms{ 0 }; // the song from its start
    presenter.show(song("One", "Spotify.exe"));
    presenter.showLyricsPage(true);
    expect(host.started.back().content.card == CardLayout::Centred && host.started.back().content.backAction == media_action::kLyricsBack,
           "the line clicked: the card becomes the lyrics' page");
    std::size_t paged = host.started.size();
    presenter.showLyricsPage(true);
    expect(host.started.size() == paged, "asked again: nothing new");
    // It follows the song: woken at each line's start, the line lit and kept in view until the user scrolls.
    expect(host.centred.size() == 1 && host.centred.back() == "lyrics:0" && host.revealed.empty() && host.followed.empty() && host.wake == Ms{ 1000 } - tokens().lyricLead,
           "opened where the song is, and woken for the first line");
    host.clock = Ms{ 1000 } - tokens().lyricLead;
    presenter.onWake();
    const Content& lit = host.started.back().content;
    expect(lit.expandedCenter.size() == 2 && std::get<Text>(lit.expandedCenter[1].items[0]).style == TextStyle::Title && host.followed.size() == 1 &&
               host.followed.back() == "lyrics:0" && !host.wake,
           "its line's start: lit, followed; nothing more to wait for after the last line");
    presenter.syncLyricsPage();
    expect(host.centred.size() == 2, "the sync button: the line being sung in the middle again");
    presenter.show(song("Two", "Spotify.exe"));
    expect(host.started.back().content.card == CardLayout::Standard, "a song with none: the card");
    presenter.show(song("One", "Spotify.exe"));
    expect(host.started.back().content.card == CardLayout::Centred, "...and the page again with the next that has some");
    presenter.showLyricsPage(false);
    expect(host.started.back().content.card == CardLayout::Standard && hasLyricsRow(host.started.back().content), "back: the card, with its line");

    // The lyrics button: off, the line goes, the cover is full size again and nothing more is asked; remembered.
    presenter.show(song("One"));
    std::size_t asked = service.asked.size();
    expect(hasLyricsRow(host.started.back().content) && host.started.back().content.smallCover, "lyrics on, a song with some: the line, a small cover");
    presenter.toggleLyrics();
    const Content& quiet = host.started.back().content;
    expect(!hasLyricsRow(quiet) && !quiet.smallCover && controlOf(quiet, media_action::kLyrics)->on == false && controlOf(quiet, media_action::kLyrics)->enabled && host.kept == "off",
           "turned off: no line, the cover full size, the button off, and it's kept");
    presenter.show(song("Four"));
    expect(service.asked.size() == asked && !hasLyricsRow(host.started.back().content), "off: a new song is not looked up");
    {
        FakeService later;
        MediaPresenter restarted(host, later.fetch());
        restarted.start();
        restarted.show(song("Five"));
        expect(later.asked.empty() && controlOf(host.started.back().content, media_action::kLyrics)->on == false, "still off after a restart");
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
               !controlOf(plainHost.started.back().content, media_action::kLyrics),
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
    pageAndPillTests();
    presenterTests();
}

} // namespace cup::test
