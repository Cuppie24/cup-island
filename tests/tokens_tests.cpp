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
}

} // namespace cup::test
