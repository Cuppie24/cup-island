// The island in the playing cover's colours (decisions 051, 054): the cover's
// colours and vividness, the palette built from it in every shipped theme, and when it shows.

#include "core/cover_palette.h"
#include "core/island_tint.h"
#include "core/tokens_json.h"
#include "providers/media/now_playing.h"
#include "expect.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace cup::test {
namespace {

// WCAG contrast ratio of a (possibly translucent) colour drawn over an opaque one.
double contrast(Color fg, Color bg)
{
    auto luminance = [](double r, double g, double b) {
        auto linear = [](double v) { v /= 255; return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b);
    };
    double a = fg.a / 255.0;
    auto blend = [&](std::uint8_t f, std::uint8_t b) { return f * a + b * (1 - a); };
    double l1 = luminance(blend(fg.r, bg.r), blend(fg.g, bg.g), blend(fg.b, bg.b)), l2 = luminance(bg.r, bg.g, bg.b);
    return (std::max(l1, l2) + 0.05) / (std::min(l1, l2) + 0.05);
}

// A 10x10 cover: `share` of its pixels (the first ones) in `main`, the rest in `rest`.
ImageData cover(Color main, Color rest, double share)
{
    ImageData image{ .width = 10, .height = 10 };
    for (int i = 0; i < 100; ++i) {
        Color c = i < share * 100 ? main : rest;
        image.bgra.insert(image.bgra.end(), { c.b, c.g, c.r, 255 });
    }
    return image;
}

// The colour's dominant channel: 0 red, 1 green, 2 blue.
int strongest(Color c) { return c.r >= c.g && c.r >= c.b ? 0 : c.g >= c.b ? 1 : 2; }

double brightness(Color c) { return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b; }

Color tokenColor(const std::vector<ThemeColor>& colors, const std::string& name)
{
    for (const ThemeColor& c : colors)
        if (c.token == name) return c.color;
    return {};
}

void coverColorTests()
{
    Color red{ 220, 30, 40 }, blue{ 30, 60, 220 }, grey{ 128, 128, 128 }, black{ 0, 0, 0 }, white{ 255, 255, 255 };
    auto solid = coverTint(cover(red, red, 1));
    expect(solid && strongest(solid->main) == 0 && !solid->second, "a red cover is red, one colour");
    expect(!coverTint(cover(grey, grey, 1)), "a grey cover has no colour: the theme stays");
    expect(!coverTint(cover(black, white, 0.5)), "black and white: no colour");
    auto logo = coverTint(cover(blue, grey, 0.2));
    expect(logo && strongest(logo->main) == 2 && !logo->second, "a blue logo on grey: blue, not grey");
    auto text = coverTint(cover(red, black, 0.3));
    expect(text && strongest(text->main) == 0, "a red picture with black around it: red");
    expect(!coverTint(cover(red, grey, 0.01)), "a speck of colour isn't enough");
    expect(!coverTint(ImageData{}), "no pixels: no colour");
    expect(!coverTint(ImageData{ .width = 10, .height = 10 }), "too few bytes for its size: no colour");

    ImageData clear = cover(red, red, 1);
    for (std::size_t i = 3; i < clear.bgra.size(); i += 4) clear.bgra[i] = 0;
    expect(!coverTint(clear), "see-through pixels don't count");

    // Two colours (decision 054): a real second hue makes a gradient; a stray one doesn't.
    auto duo = coverTint(cover(red, blue, 0.6));
    expect(duo && strongest(duo->main) == 0 && duo->second && strongest(*duo->second) == 2, "red and blue: red, then blue");
    auto stray = coverTint(cover(red, blue, 0.9));
    expect(stray && !stray->second, "a little blue in a red cover: one colour");
    Color orange{ 230, 110, 30 };
    auto neighbours = coverTint(cover(red, orange, 0.5));
    expect(neighbours && !neighbours->second, "red and orange are one hue's neighbourhood: no gradient");

    // Vividness: a neon cover is fully vivid, a dusty one much less.
    auto neon = coverTint(cover({ 255, 0, 60 }, { 255, 0, 60 }, 1));
    auto dusty = coverTint(cover({ 150, 110, 105 }, { 150, 110, 105 }, 1));
    expect(neon && neon->vividness > 0.9f, "a neon cover: fully vivid");
    expect(dusty && dusty->vividness < 0.5f, "a dusty cover: muted");
}

void paletteTests()
{
    std::ifstream in(CUP_TOKENS_FILE);
    std::string shipped{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
    TokensParse parsed = parseTokens(shipped);
    expect(parsed.ok(), "assets/tokens.json parses");

    // Every shipped theme, around every hue: dark stays dark, light stays
    // light, the island leans to the cover's hue, and text stays readable.
    for (const Theme& theme : parsed.tokens.themes) {
        Tokens themed = withTheme(parsed.tokens, theme.name);
        for (Color c : { Color{ 230, 40, 40 }, Color{ 240, 200, 30 }, Color{ 40, 200, 70 }, Color{ 30, 200, 220 },
                         Color{ 40, 70, 230 }, Color{ 200, 40, 220 } }) {
            Tokens tinted = withColors(themed, coverColors({ .main = c, .second = Color{ c.b, c.r, c.g }, .vividness = 1 }, themed));
            std::string what = theme.name + " around " + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b);
            expect((brightness(tinted.islandColor) > 127.5) == theme.light, "keeps its lightness: " + what);
            for (Color under : { tinted.islandColor, tinted.islandSecondColor }) // both ends of the gradient
                expect(contrast(tinted.titleTextColor, under) >= 7 && contrast(tinted.bodyTextColor, under) >= 4.5 &&
                           contrast(tinted.captionTextColor, under) >= 3,
                       "readable text: " + what);
            expect(contrast(tinted.liveColor, tinted.islandColor) >= 2, "the accent shows on the island: " + what);
            expect(tinted.shadowColor == themed.shadowColor && tinted.dangerColor == themed.dangerColor,
                   "the shadow and danger red stay the theme's: " + what);
        }
    }

    Tokens midnight = withTheme(parsed.tokens, "Midnight");
    auto red = coverColors({ .main = { 220, 30, 40 }, .vividness = 1 }, midnight);
    Color island = tokenColor(red, "islandColor");
    expect(island.r > island.b && island.r > island.g && brightness(island) < 60, "a dark theme: a near-black tinted red");
    expect(tokenColor(red, "bodyTextColor").a == midnight.bodyTextColor.a, "opacities stay the theme's");
    expect(strongest(tokenColor(red, "liveColor")) == 0, "the accent is the cover's colour");

    Tokens scaled = scaleTokens(parsed.tokens, 2);
    expect(scaled.coverDarkIsland == parsed.tokens.coverDarkIsland && scaled.coverIslandChroma == parsed.tokens.coverIslandChroma,
           "lightnesses and chromas aren't lengths: not scaled");
    expect(tokenColor(red, "islandSecondColor") == island, "one colour: the island's ends match");

    // Vividness sets the strength: a vivid cover, a richer island than a muted one of the same hue.
    auto chromaOf = [](Color c) { return std::max({ c.r, c.g, c.b }) - std::min({ c.r, c.g, c.b }); };
    Color vividIsland = tokenColor(coverColors({ .main = { 220, 30, 40 }, .vividness = 1 }, midnight), "islandColor");
    Color mutedIsland = tokenColor(coverColors({ .main = { 220, 30, 40 }, .vividness = 0 }, midnight), "islandColor");
    expect(chromaOf(vividIsland) > chromaOf(mutedIsland), "vivid cover, richer island");
    auto duo = coverColors({ .main = { 220, 30, 40 }, .second = Color{ 30, 60, 220 }, .vividness = 1 }, midnight);
    expect(strongest(tokenColor(duo, "islandSecondColor")) == 2 && strongest(tokenColor(duo, "islandColor")) == 0,
           "two colours: the island runs from red to blue");
}

void tintTests()
{
    Ms t{ 10000 };
    CoverTint red{ .main = { 200, 0, 0 } }, blue{ .main = { 0, 0, 200 } };
    IslandState state;
    state.activities.push_back({ .id = "timer" });
    state.activities.push_back({ .id = "media", .content = { .tint = red } });
    expect(wantedTint(state) == WantedTint{ red }, "the best-ranked activity asking for a tint gets it, past ones that don't");
    expect(wantedTint(IslandState{}) == WantedTint{}, "nothing playing: no tint");
    state.activities[1].content = { .tintComing = true };
    expect(wantedTint(state) == WantedTint{ .coming = true }, "a new track's cover not read yet: a colour is coming");

    IslandTint tint = nextTint({}, { red }, t);
    expect(tint.shown == red && !tintDeadline(tint), "a cover's colour shows at once");
    tint = nextTint(tint, { blue }, t);
    expect(tint.shown == blue, "the next track's colour too");
    expect(nextTint(tint, {}, t) == IslandTint{}, "paused (decision 058): the theme at once, no wait");

    // Skipping: the new track shows before its cover is read.
    IslandTint skipping = nextTint(tint, { .coming = true }, t);
    expect(skipping.shown == blue && tintDeadline(skipping) == t + tokens().coverTintLinger,
           "a new track's colour coming: the old one stays meanwhile");
    expect(nextTint(skipping, { red }, t + Ms{ 500 }) == IslandTint{ red, std::nullopt }, "its cover read: its colour, no flash");
    expect(nextTint(skipping, { .coming = true }, t + Ms{ 500 }) == skipping, "still coming: the wait doesn't restart");
    expect(!nextTint(skipping, { .coming = true }, t + tokens().coverTintLinger).shown, "never came: the theme's colours");
    expect(nextTint(skipping, {}, t + Ms{ 500 }) == IslandTint{}, "paused while waiting: the theme at once");
    expect(nextTint({}, { .coming = true }, t) == IslandTint{} && nextTint({}, {}, t) == IslandTint{}, "no tint stays none");

    // The content side.
    NowPlaying track{ .title = "A" };
    track.timeline.playing = true;
    expect(nowPlayingContent(track).tintComing && !nowPlayingContent(track).tint, "playing, no cover yet: a tint is coming");
    track.timeline.playing = false;
    expect(!nowPlayingContent(track).tintComing, "paused: nothing coming");
}

void nowPlayingTests()
{
    auto art = std::make_shared<const ImageData>(cover({ 220, 30, 40 }, { 220, 30, 40 }, 1));
    NowPlaying playing{ .title = "T", .art = art };
    playing.timeline.playing = true;
    expect(nowPlayingContent(playing).tint.has_value(), "playing with a coloured cover: the island takes it on");
    NowPlaying paused = playing;
    paused.timeline.playing = false;
    expect(!nowPlayingContent(paused).tint, "paused: the theme");
    NowPlaying bare = playing;
    bare.art = nullptr;
    expect(!nowPlayingContent(bare).tint, "no cover: the theme");
}

} // namespace

void runCoverTests()
{
    coverColorTests();
    paletteTests();
    tintTests();
    nowPlayingTests();
}

} // namespace cup::test
