// The island in the playing cover's colours (decision 051): the cover's main
// colour, the palette built from it in every shipped theme, and when it shows.

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
    auto solid = coverColor(cover(red, red, 1));
    expect(solid && strongest(*solid) == 0, "a red cover is red");
    expect(!coverColor(cover(grey, grey, 1)), "a grey cover has no colour: the theme stays");
    expect(!coverColor(cover(black, white, 0.5)), "black and white: no colour");
    auto logo = coverColor(cover(blue, grey, 0.2));
    expect(logo && strongest(*logo) == 2, "a blue logo on grey: blue, not grey");
    auto text = coverColor(cover(red, black, 0.3));
    expect(text && strongest(*text) == 0, "a red picture with black around it: red");
    expect(!coverColor(cover(red, grey, 0.01)), "a speck of colour isn't enough");
    expect(!coverColor(ImageData{}), "no pixels: no colour");
    expect(!coverColor(ImageData{ .width = 10, .height = 10 }), "too few bytes for its size: no colour");

    ImageData clear = cover(red, red, 1);
    for (std::size_t i = 3; i < clear.bgra.size(); i += 4) clear.bgra[i] = 0;
    expect(!coverColor(clear), "see-through pixels don't count");
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
            Tokens tinted = withColors(themed, coverColors(c, themed));
            std::string what = theme.name + " around " + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b);
            expect((brightness(tinted.islandColor) > 127.5) == theme.light, "keeps its lightness: " + what);
            expect(contrast(tinted.titleTextColor, tinted.islandColor) >= 7 && contrast(tinted.bodyTextColor, tinted.islandColor) >= 4.5 &&
                       contrast(tinted.captionTextColor, tinted.islandColor) >= 3,
                   "readable text: " + what);
            expect(contrast(tinted.liveColor, tinted.islandColor) >= 2, "the accent shows on the island: " + what);
            expect(tinted.shadowColor == themed.shadowColor && tinted.dangerColor == themed.dangerColor,
                   "the shadow and danger red stay the theme's: " + what);
        }
    }

    Tokens midnight = withTheme(parsed.tokens, "Midnight");
    auto red = coverColors({ 220, 30, 40 }, midnight);
    Color island = tokenColor(red, "islandColor");
    expect(island.r > island.b && island.r > island.g && brightness(island) < 60, "a dark theme: a near-black tinted red");
    expect(tokenColor(red, "bodyTextColor").a == midnight.bodyTextColor.a, "opacities stay the theme's");
    expect(strongest(tokenColor(red, "liveColor")) == 0, "the accent is the cover's colour");

    Tokens scaled = scaleTokens(parsed.tokens, 2);
    expect(scaled.coverDarkIsland == parsed.tokens.coverDarkIsland, "lightnesses aren't lengths: not scaled");
}

void tintTests()
{
    Ms t{ 10000 };
    Color red{ 200, 0, 0 }, blue{ 0, 0, 200 };
    IslandState state;
    state.activities.push_back({ .id = "timer" });
    state.activities.push_back({ .id = "media", .content = { .tint = red } });
    expect(wantedTint(state) == red, "the best-ranked activity asking for a tint gets it, past ones that don't");
    expect(!wantedTint(IslandState{}), "nothing playing: no tint");

    IslandTint tint = nextTint({}, red, t);
    expect(tint.shown == red && !tintDeadline(tint), "a cover's colour shows at once");
    tint = nextTint(tint, blue, t);
    expect(tint.shown == blue, "the next track's colour too");
    tint = nextTint(tint, std::nullopt, t);
    expect(tint.shown == blue && tintDeadline(tint) == t + tokens().coverTintLinger, "paused: the colour lingers");
    expect(nextTint(tint, blue, t + Ms{ 500 }) == IslandTint{ blue, std::nullopt }, "playing again within the linger: no flash");
    expect(nextTint(tint, std::nullopt, t + Ms{ 500 }) == tint, "still paused: the linger doesn't restart");
    expect(!nextTint(tint, std::nullopt, t + tokens().coverTintLinger).shown, "the linger over: the theme's colours");
    expect(nextTint({}, std::nullopt, t) == IslandTint{}, "no tint stays none");
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
