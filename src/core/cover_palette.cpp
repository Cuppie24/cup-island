#include "core/cover_palette.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cup {
namespace {

constexpr double kPi = 3.14159265358979323846;

// --- OKLab / OKLCH (Björn Ottosson's): lightness and chroma that look even
// across hues, so "darker" doesn't drift a hue toward brown or grey.
struct Lch {
    double l = 0, c = 0, h = 0; // h in radians
};

double toLinear(std::uint8_t channel)
{
    double c = channel / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double toSrgb(double linear)
{
    double c = linear <= 0.0031308 ? 12.92 * linear : 1.055 * std::pow(linear, 1 / 2.4) - 0.055;
    return c * 255;
}

Lch toLch(Color color)
{
    double r = toLinear(color.r), g = toLinear(color.g), b = toLinear(color.b);
    double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    double labA = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s;
    double labB = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s;
    return { 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s, std::hypot(labA, labB), std::atan2(labB, labA) };
}

// Linear sRGB of an OKLCH colour; outside 0..1 when sRGB can't show it.
std::array<double, 3> toLinearRgb(Lch c)
{
    double labA = c.c * std::cos(c.h), labB = c.c * std::sin(c.h);
    double l = c.l + 0.3963377774 * labA + 0.2158037573 * labB;
    double m = c.l - 0.1055613458 * labA - 0.0638541728 * labB;
    double s = c.l - 0.0894841775 * labA - 1.2914855480 * labB;
    l = l * l * l, m = m * m * m, s = s * s * s;
    return { 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
             -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
             -0.0042960863 * l - 0.7034186147 * m + 1.7076147010 * s };
}

bool inGamut(Lch c)
{
    for (double v : toLinearRgb(c))
        if (v < -1e-4 || v > 1 + 1e-4) return false;
    return true;
}

// The colour at this lightness and hue, as close to `chroma` as sRGB allows:
// a pale or very dark colour can't be as vivid, so it loses chroma, never hue.
Color fromLch(double lightness, double chroma, double hue, std::uint8_t alpha)
{
    Lch c{ std::clamp(lightness, 0.0, 1.0), std::max(chroma, 0.0), hue };
    if (!inGamut(c)) {
        double low = 0, high = c.c;
        for (int i = 0; i < 20; ++i) {
            double mid = (low + high) / 2;
            (inGamut({ c.l, mid, hue }) ? low : high) = mid;
        }
        c.c = low;
    }
    auto channel = [](double linear) {
        return static_cast<std::uint8_t>(std::clamp(std::lround(toSrgb(std::clamp(linear, 0.0, 1.0))), 0L, 255L));
    };
    auto rgb = toLinearRgb(c);
    return { channel(rgb[0]), channel(rgb[1]), channel(rgb[2]), alpha };
}

// Light or dark the way Settings files themes (tokens_json.cpp): by the island.
bool isLight(Color island) { return 0.2126f * island.r + 0.7152f * island.g + 0.0722f * island.b > 127.5f; }

// Which tokens are text-coloured (ink) and which the accent, per decision
// 022's recipe. Light themes draw empty tracks, fields and placeholders in
// faint ink; dark ones in a faint accent.
struct Role {
    const char* name;
    Color Tokens::*member;
};
constexpr Role kInk[] = {
    { "titleTextColor", &Tokens::titleTextColor },   { "bodyTextColor", &Tokens::bodyTextColor },
    { "captionTextColor", &Tokens::captionTextColor }, { "iconColor", &Tokens::iconColor },
    { "buttonColor", &Tokens::buttonColor },         { "buttonOffColor", &Tokens::buttonOffColor },
    { "buttonDisabledColor", &Tokens::buttonDisabledColor }, { "knobColor", &Tokens::knobColor },
    { "stepDoneColor", &Tokens::stepDoneColor },     { "placeholderTextColor", &Tokens::placeholderTextColor },
};
constexpr Role kAccent[] = {
    { "buttonHoverColor", &Tokens::buttonHoverColor }, { "liveColor", &Tokens::liveColor },
    { "tabSelectedColor", &Tokens::tabSelectedColor }, { "hoverColor", &Tokens::hoverColor },
    { "tileColor", &Tokens::tileColor },               { "focusRingColor", &Tokens::focusRingColor },
};
constexpr Role kFaint[] = { // ink on light themes, accent on dark ones
    { "trackColor", &Tokens::trackColor },   { "stepTodoColor", &Tokens::stepTodoColor },
    { "inputColor", &Tokens::inputColor },   { "imagePlaceholderColor", &Tokens::imagePlaceholderColor },
};

} // namespace

std::optional<CoverTint> coverTint(const ImageData& image)
{
    const Tokens& t = tokens();
    std::size_t count = static_cast<std::size_t>(std::max(image.width, 0)) * static_cast<std::size_t>(std::max(image.height, 0));
    if (count == 0 || image.bgra.size() < count * 4) return std::nullopt;

    // Hues in 15° bins; each keeps its pixels' colour sum, weighted.
    constexpr int kBins = 24;
    // Darker than this, a pixel's hue is mostly noise (JPEG shadows).
    constexpr double kDarkest = 0.2;
    struct Bin {
        double weight = 0, r = 0, g = 0, b = 0;
    };
    std::array<Bin, kBins> bins{};
    std::size_t opaque = 0, vivid = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* p = &image.bgra[i * 4];
        if (p[3] < 128) continue; // see-through: not part of the picture
        ++opaque;
        double alpha = p[3] / 255.0; // premultiplied: undo it
        double r = std::min(255.0, p[2] / alpha), g = std::min(255.0, p[1] / alpha), b = std::min(255.0, p[0] / alpha);
        double high = std::max({ r, g, b }), low = std::min({ r, g, b });
        double value = high / 255, saturation = high > 0 ? (high - low) / high : 0;
        if (value < kDarkest || saturation < t.coverVividSaturation) continue;
        ++vivid;

        double hue; // degrees, 0..360
        double span = high - low;
        if (high == r) hue = std::fmod((g - b) / span + 6, 6.0) * 60;
        else if (high == g) hue = ((b - r) / span + 2) * 60;
        else hue = ((r - g) / span + 4) * 60;
        Bin& bin = bins[static_cast<int>(hue / 360 * kBins) % kBins];
        double weight = saturation * value; // vivid, bright pixels are the ones you notice
        bin.weight += weight, bin.r += r * weight, bin.g += g * weight, bin.b += b * weight;
    }
    if (opaque == 0 || vivid == 0 || static_cast<double>(vivid) < t.coverMinVivid * static_cast<double>(opaque)) return std::nullopt;

    // A hue on a bin's edge splits between two: score each bin with its neighbours.
    auto at = [&](int i) -> const Bin& { return bins[(i + kBins) % kBins]; };
    auto score = [&](int i) { return at(i - 1).weight + at(i).weight + at(i + 1).weight; };
    auto colourAround = [&](int centre) {
        Bin sum;
        for (int i = centre - 1; i <= centre + 1; ++i)
            sum.weight += at(i).weight, sum.r += at(i).r, sum.g += at(i).g, sum.b += at(i).b;
        auto channel = [&](double v) { return static_cast<std::uint8_t>(std::clamp(std::lround(v / sum.weight), 0L, 255L)); };
        return Color{ channel(sum.r), channel(sum.g), channel(sum.b) };
    };
    int best = 0;
    for (int i = 1; i < kBins; ++i)
        if (score(i) > score(best)) best = i;

    // A second hue: the strongest window not overlapping the main one (3 bins,
    // 45°, away), if it's a real part of the cover and not a stray accent.
    std::optional<int> second;
    for (int i = 0; i < kBins; ++i) {
        int apart = std::abs(i - best);
        if (std::min(apart, kBins - apart) < 3) continue;
        if (!second || score(i) > score(*second)) second = i;
    }
    if (second && score(*second) < t.coverSecondShare * score(best)) second.reset();

    CoverTint tint{ .main = colourAround(best) };
    if (second) tint.second = colourAround(*second);
    tint.vividness = static_cast<float>(std::clamp(toLch(tint.main).c / std::max<double>(t.coverFullChroma, 1e-3), 0.0, 1.0));
    return tint;
}

std::vector<ThemeColor> coverColors(const CoverTint& tint, const Tokens& themed)
{
    const Tokens& t = themed;
    bool light = isLight(t.islandColor);
    double hue = toLch(tint.main).h;
    // As strong as the cover is vivid: a neon cover a rich island, a dusty one a quiet one.
    double vivid = std::clamp(static_cast<double>(tint.vividness), 0.0, 1.0);
    auto strength = [&](Vec2 range) { return range.x + (range.y - range.x) * vivid; };
    double islandChroma = strength(t.coverIslandChroma);
    double islandL = light ? t.coverLightIsland : t.coverDarkIsland;
    // The highlight (a drop target, decision 007) is as far from the island as the theme's own.
    double highlightL = islandL + toLch(t.islandHighlightColor).l - toLch(t.islandColor).l;
    Color island = fromLch(islandL, islandChroma, hue, 255);
    Color second = tint.second ? fromLch(islandL, islandChroma, toLch(*tint.second).h, 255) : island;
    Color highlight = fromLch(highlightL, islandChroma, hue, 255);
    Color ink = fromLch(light ? t.coverLightInk : t.coverDarkInk, t.coverInkChroma, hue, 255);
    Color accent = fromLch(light ? t.coverLightAccent : t.coverDarkAccent, strength(t.coverAccentChroma), hue, 255);

    std::vector<ThemeColor> out{ { "islandColor", island }, { "islandSecondColor", second }, { "islandHighlightColor", highlight } };
    // Each keeps the theme's opacity: only the colour changes, not how strong it is.
    auto add = [&](const Role& role, Color colour) {
        colour.a = (t.*role.member).a;
        out.push_back({ role.name, colour });
    };
    for (const Role& role : kInk) add(role, ink);
    for (const Role& role : kAccent) add(role, accent);
    for (const Role& role : kFaint) add(role, light ? ink : accent);
    return out;
}

} // namespace cup
