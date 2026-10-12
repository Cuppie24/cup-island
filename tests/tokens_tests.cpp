// tokens.json parsing: round trip, partial files, and every kind of mistake
// a person tuning values by hand is likely to make.

#include "core/tokens_json.h"
#include "expect.h"

#include <fstream>
#include <iterator>
#include <string>

namespace cup::test {
namespace {

bool mentions(const TokensParse& p, const std::string& text)
{
    for (const auto& problem : p.problems)
        if (problem.find(text) != std::string::npos) return true;
    return false;
}

} // namespace

void runTokensTests()
{
    // Round trip: what toJson writes, parseTokens reads back identically.
    TokensParse roundTrip = parseTokens(toJson(Tokens{}));
    expect(roundTrip.ok() && roundTrip.tokens == Tokens{}, "toJson -> parseTokens round-trips the defaults");

    // The shipped file is valid: no typos, no stale names.
    std::ifstream in(CUP_TOKENS_FILE);
    std::string shipped{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
    TokensParse fromFile = parseTokens(shipped);
    expect(!shipped.empty() && fromFile.ok(), "assets/tokens.json parses without problems");
    for (const auto& problem : fromFile.problems) expect(false, "assets/tokens.json: " + problem);

    // Partial file: given keys apply, the rest keep defaults.
    TokensParse partial = parseTokens(R"({ "morphDurationMs": 450, "expandedSize": [400, 120],
        "morphEase": [0.2, 0, 0, 1] })");
    expect(partial.ok(), "partial file is fine");
    expect(partial.tokens.morphDuration == Ms{ 450 }, "duration applied");
    expect(partial.tokens.expandedSize == Vec2{ 400, 120 }, "size applied");
    expect(partial.tokens.morphEase == Bezier{ { 0.2f, 0 }, { 0, 1 } }, "easing applied");
    expect(partial.tokens.alertDuration == Tokens{}.alertDuration, "untouched keys keep defaults");
    expect(parseTokens("{}").ok(), "empty object is fine");

    // Mistakes are reported with a line number and nothing is applied.
    TokensParse typo = parseTokens("{\n  \"morphDurationMS\": 400\n}");
    expect(!typo.ok() && mentions(typo, "line 2") && mentions(typo, "unknown token"), "unknown key is reported with its line");
    expect(mentions(parseTokens(R"({"expandedSize": 400})"), "expected [width, height]"), "size needs two numbers");
    expect(mentions(parseTokens(R"({"morphDurationMs": [300]})"), "expected a number"), "duration needs a number");
    expect(mentions(parseTokens(R"({"alertDurationMs": -1})"), "must not be negative"), "negative duration rejected");
    expect(mentions(parseTokens(R"({"morphEase": [1.5, 0, 0, 1]})"), "within 0..1"), "bezier x out of range rejected");
    expect(mentions(parseTokens(R"({"alertQueueCapacity": 0})"), "whole number >= 1"), "queue capacity must be >= 1");
    expect(mentions(parseTokens(R"({"pressScale": 1, "pressScale": 2})"), "appears twice"), "duplicate key rejected");
    expect(mentions(parseTokens("{\n\"pressScale\": 0.9\n\"alertDurationMs\": 1}"), "line 3: expected '}'"), "missing comma is a syntax error");
    expect(mentions(parseTokens(R"({"pressScale": abc})"), "expected a number"), "non-number rejected");
    expect(!parseTokens("").ok(), "empty file rejected");
    expect(!parseTokens(R"({"pressScale": 1} extra)").ok(), "trailing text rejected");
    expect(parseTokens(R"({"alertSize": [300, 60]})").tokens.alertSize == Vec2{ 300, 60 }, "alert size applied");

    // Colours: [r, g, b] opaque, or [r, g, b, alpha] with alpha 0..1, like CSS rgba().
    TokensParse colours = parseTokens(R"({ "islandColor": [10, 20, 30], "trackColor": [255, 255, 255, 0.5] })");
    expect(colours.ok() && colours.tokens.islandColor == Color{ 10, 20, 30, 255 }, "rgb colour applied, opaque");
    expect(colours.tokens.trackColor == Color{ 255, 255, 255, 128 }, "alpha 0..1 applied");
    expect(mentions(parseTokens(R"({"islandColor": [0, 0]})"), "expected [r, g, b]"), "colour needs 3 or 4 numbers");
    expect(mentions(parseTokens(R"({"islandColor": [0, 0, 256]})"), "0..255"), "channel out of range rejected");
    expect(mentions(parseTokens(R"({"islandColor": [0, 0, 1.5]})"), "0..255"), "fractional channel rejected");
    expect(mentions(parseTokens(R"({"islandColor": [0, 0, 0, 2]})"), "alpha must be within 0..1"), "alpha out of range rejected");
    // The themes live only in tokens.json: tokens.h has no copy of their colours.
    Tokens shippedPlain = fromFile.tokens;
    shippedPlain.themes.clear();
    expect(shippedPlain == Tokens{}, "the shipped file matches the defaults in tokens.h");

    // Themes (decision 022): name -> the colour tokens it changes. Dark only, so
    // every shipped one but the plain default darkens the island and keeps text light.
    const auto& shippedThemes = fromFile.tokens.themes;
    expect(shippedThemes.size() >= 2 && shippedThemes[0].name == "Midnight" && shippedThemes[0].colors.empty(), "the default theme is the colours as written");
    for (const Theme& theme : shippedThemes) {
        Tokens themed = withTheme(fromFile.tokens, theme.name);
        auto light = [](Color c) { return c.r + c.g + c.b; };
        expect(light(themed.islandColor) < 150 && light(themed.titleTextColor) > 600, "a dark theme: " + theme.name);
        // The shadow stays black in every theme: a coloured one reads as a halo.
        expect(themed.shadowColor == fromFile.tokens.shadowColor, "a black shadow: " + theme.name);
    }
    TokensParse themes = parseTokens(R"({ "themes": {
        "Plain": {},
        "Grape": { "islandColor": [24, 9, 42], "liveColor": [191, 90, 242, 0.5] } } })");
    expect(themes.ok() && themes.tokens.themes.size() == 2 && themes.tokens.themes[1].name == "Grape", "themes parse, in order");
    Tokens grape = withTheme(themes.tokens, "Grape");
    expect(grape.islandColor == Color{ 24, 9, 42 } && grape.liveColor == Color{ 191, 90, 242, 128 }, "a theme's colours apply");
    expect(grape.titleTextColor == Tokens{}.titleTextColor && grape.themes == themes.tokens.themes, "the rest stay as written");
    expect(withTheme(themes.tokens, "Nope") == themes.tokens && withTheme(themes.tokens, "Plain") == themes.tokens,
           "an unknown or empty theme changes nothing");
    TokensParse themesBack = parseTokens(toJson(themes.tokens));
    expect(themesBack.ok() && themesBack.tokens == themes.tokens, "themes round-trip through toJson");
    expect(mentions(parseTokens("{\n\"themes\": {\n\"A\": { \"tileHeight\": 4 } } }"), "line 3") &&
           mentions(parseTokens(R"({"themes": {"A": {"tileHeight": 4}}})"), "not a colour token"), "a theme only recolours");
    expect(mentions(parseTokens(R"({"themes": {"A": {"islandColr": [0, 0, 0]}}})"), "unknown token"), "a typo in a theme is reported");
    expect(mentions(parseTokens(R"({"themes": {"A": {"islandColor": [0, 0, 300]}}})"), "0..255"), "a theme's colours are checked");
    expect(mentions(parseTokens(R"({"themes": {"A": {}, "A": {}}})"), "appears twice"), "theme names are unique");
    expect(mentions(parseTokens(R"({"themes": {"": {}}})"), "needs a name"), "a theme needs a name");
    expect(mentions(parseTokens(R"({"themes": {"A": {"islandColor": [0, 0, 0], "islandColor": [1, 1, 1]}}})"), "appears twice"),
           "a colour appears once per theme");
    expect(!parseTokens(R"({"themes": [1, 2]})").ok(), "themes must be an object");

    // Display scaling: lengths grow with the display's scale times islandScale; nothing else does.
    Tokens d{};
    float k = 1.5f * d.islandScale;
    Tokens x = scaleTokens(d, 1.5f);
    expect(x.expandedSize == Vec2{ d.expandedSize.x * k, d.expandedSize.y * k } && x.canvas.y == d.canvas.y * k, "sizes scale");
    expect(x.bodyFontSize == d.bodyFontSize * k && x.expandedPadding == d.expandedPadding * k &&
           x.dragStartDistance == d.dragStartDistance * k && x.marqueeSpeed == d.marqueeSpeed * k,
           "fonts, paddings, distances and px/s speeds scale");
    expect(x.pressScale == d.pressScale && x.buttonPressScale == d.buttonPressScale && x.islandScale == d.islandScale,
           "factors don't, the size knob included");
    expect(x.morphDuration == d.morphDuration && x.morphEase == d.morphEase && x.alertQueueCapacity == d.alertQueueCapacity &&
           x.islandColor == d.islandColor, "durations, curves, counts and colours don't");
    Tokens asWritten = d;
    asWritten.islandScale = 1.0f;
    expect(scaleTokens(asWritten, 1.0f) == asWritten, "100% and islandScale 1: the sizes as written");
    expect(scaleTokens(d, 1.0f).expandedSize.x == d.expandedSize.x * d.islandScale, "islandScale alone makes it more compact");
}

} // namespace cup::test
