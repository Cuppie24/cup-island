#include "core/tokens_json.h"

#include "core/json_reader.h"

#include <cmath>
#include <set>
#include <sstream>
#include <type_traits>
#include <variant>

namespace cup {
namespace {

// --- Field table: JSON name <-> Tokens member ------------------------------

using Member = std::variant<float Tokens::*, Vec2 Tokens::*, Ms Tokens::*, std::size_t Tokens::*, Bezier Tokens::*, Color Tokens::*>;

struct Field {
    const char* name;
    Member member;
    bool startsGroup = false; // blank line before it in toJson()
};

const Field kFields[] = {
    { "canvasSize", &Tokens::canvas },
    { "islandScale", &Tokens::islandScale },
    { "idleSize", &Tokens::idleSize, true },
    { "compactSize", &Tokens::compactSize },
    { "alertSize", &Tokens::alertSize },
    { "expandedSize", &Tokens::expandedSize },
    { "receptiveSize", &Tokens::receptiveSize },
    { "hiddenSize", &Tokens::hiddenSize },
    { "maxCornerRadius", &Tokens::maxCornerRadius },
    { "flareRadius", &Tokens::flareRadius },
    { "compactInset", &Tokens::compactInset, true },
    { "alertPadding", &Tokens::alertPadding },
    { "expandedPadding", &Tokens::expandedPadding },
    { "listCardInset", &Tokens::listCardInset },
    { "listCardWidth", &Tokens::listCardWidth },
    { "listCardVerticalInset", &Tokens::listCardVerticalInset },
    { "listCardVisibleRows", &Tokens::listCardVisibleRows },
    { "listCardMinHeight", &Tokens::listCardMinHeight },
    { "expandedArtSize", &Tokens::expandedArtSize },
    { "slotGap", &Tokens::slotGap },
    { "rowSpacing", &Tokens::rowSpacing },
    { "itemSpacing", &Tokens::itemSpacing },
    { "looseItemSpacing", &Tokens::looseItemSpacing },
    { "titleFontSize", &Tokens::titleFontSize },
    { "bodyFontSize", &Tokens::bodyFontSize },
    { "captionFontSize", &Tokens::captionFontSize },
    { "displayFontSize", &Tokens::displayFontSize },
    { "headingFontSize", &Tokens::headingFontSize },
    { "inputFontSize", &Tokens::inputFontSize },
    { "titleFontWeight", &Tokens::titleFontWeight },
    { "bodyFontWeight", &Tokens::bodyFontWeight },
    { "captionFontWeight", &Tokens::captionFontWeight },
    { "displayFontWeight", &Tokens::displayFontWeight },
    { "headingFontWeight", &Tokens::headingFontWeight },
    { "textContrast", &Tokens::textContrast },
    { "iconSize", &Tokens::iconSize },
    { "buttonSize", &Tokens::buttonSize },
    { "smallButtonSize", &Tokens::smallButtonSize },
    { "revealGlyphSize", &Tokens::revealGlyphSize },
    { "inputHeight", &Tokens::inputHeight },
    { "listItemPadding", &Tokens::listItemPadding },
    { "listItemInset", &Tokens::listItemInset },
    { "listItemTextInset", &Tokens::listItemTextInset },
    { "sectionGap", &Tokens::sectionGap },
    { "editInset", &Tokens::editInset },
    { "buttonLabelPadding", &Tokens::buttonLabelPadding },
    { "buttonGlyphSize", &Tokens::buttonGlyphSize },
    { "buttonPressScale", &Tokens::buttonPressScale },
    { "progressHeight", &Tokens::progressHeight },
    { "progressHitHeight", &Tokens::progressHitHeight },
    { "progressKnobSize", &Tokens::progressKnobSize },
    { "timeLabelWidth", &Tokens::timeLabelWidth },
    { "ringThickness", &Tokens::ringThickness },
    { "stepDotSize", &Tokens::stepDotSize },
    { "stepDotGap", &Tokens::stepDotGap },
    { "badgeSize", &Tokens::badgeSize },
    { "badgeInset", &Tokens::badgeInset },
    { "swatchSize", &Tokens::swatchSize },
    { "switchSize", &Tokens::switchSize },
    { "switchKnobInset", &Tokens::switchKnobInset },
    { "tagHeight", &Tokens::tagHeight },
    { "tagPadding", &Tokens::tagPadding },
    { "hoverFadeDurationMs", &Tokens::hoverFadeDuration, true },
    { "artHoverGlyphSize", &Tokens::artHoverGlyphSize },
    { "marqueeSpeed", &Tokens::marqueeSpeed, true },
    { "marqueePauseMs", &Tokens::marqueePause },
    { "marqueeGap", &Tokens::marqueeGap },
    { "marqueeFadeWidth", &Tokens::marqueeFadeWidth },
    { "waveformBarCount", &Tokens::waveformBarCount, true },
    { "waveformBarWidth", &Tokens::waveformBarWidth },
    { "waveformBarGap", &Tokens::waveformBarGap },
    { "waveformPeriodMs", &Tokens::waveformPeriod },
    { "bubbleSize", &Tokens::bubbleSize, true },
    { "bubbleGap", &Tokens::bubbleGap },
    { "dragStartDistance", &Tokens::dragStartDistance },
    { "bridgeThickness", &Tokens::bridgeThickness },
    { "bridgeReach", &Tokens::bridgeReach },
    { "liftHoldMs", &Tokens::liftHold, true },
    { "liftDrop", &Tokens::liftDrop },
    { "centreSnap", &Tokens::centreSnap },
    { "edgeSnap", &Tokens::edgeSnap },
    { "shadowBlur", &Tokens::shadowBlur, true },
    { "shadowOffset", &Tokens::shadowOffset },
    { "tabHeight", &Tokens::tabHeight, true },
    { "tabGap", &Tokens::tabGap },
    { "tabMaxWidth", &Tokens::tabMaxWidth },
    { "homeButtonWidth", &Tokens::homeButtonWidth },
    { "homeColumns", &Tokens::homeColumns, true },
    { "tileHeight", &Tokens::tileHeight },
    { "tileGap", &Tokens::tileGap },
    { "tilePadding", &Tokens::tilePadding },
    { "focusRingWidth", &Tokens::focusRingWidth },
    { "dropZoneGap", &Tokens::dropZoneGap },
    { "morphDurationMs", &Tokens::morphDuration, true },
    { "morphEase", &Tokens::morphEase },
    { "contentFadeDurationMs", &Tokens::contentFadeDuration },
    { "contentFadeDelayMs", &Tokens::contentFadeDelay },
    { "recolorFastMs", &Tokens::recolorFast },
    { "recolorMediumMs", &Tokens::recolorMedium },
    { "recolorSlowMs", &Tokens::recolorSlow },
    { "pressScale", &Tokens::pressScale, true },
    { "pressDownDurationMs", &Tokens::pressDownDuration },
    { "pressDownEase", &Tokens::pressDownEase },
    { "pressReleaseDurationMs", &Tokens::pressReleaseDuration },
    { "pressReleaseEase", &Tokens::pressReleaseEase },
    { "alertDurationMs", &Tokens::alertDuration, true },
    { "expandedIdleTimeoutMs", &Tokens::expandedIdleTimeout },
    { "dropLeaveGraceMs", &Tokens::dropLeaveGrace },
    { "heldAlertMaxAgeMs", &Tokens::heldAlertMaxAge },
    { "alertQueueCapacity", &Tokens::alertQueueCapacity },
    { "mediaPausedLingerMs", &Tokens::mediaPausedLinger, true },
    { "mediaSessionRecheckMs", &Tokens::mediaSessionRecheck },
    { "mediaArtFadeMs", &Tokens::mediaArtFade },
    { "mediaArtRecheckMs", &Tokens::mediaArtRecheck },
    { "mediaReadStuckMs", &Tokens::mediaReadStuck },
    { "fullscreenRecheckMs", &Tokens::fullscreenRecheck },
    { "mediaReadRetries", &Tokens::mediaReadRetries },
    { "coverTintLingerMs", &Tokens::coverTintLinger, true },
    { "coverVividSaturation", &Tokens::coverVividSaturation },
    { "coverMinVivid", &Tokens::coverMinVivid },
    { "coverDarkIsland", &Tokens::coverDarkIsland },
    { "coverDarkAccent", &Tokens::coverDarkAccent },
    { "coverDarkInk", &Tokens::coverDarkInk },
    { "coverLightIsland", &Tokens::coverLightIsland },
    { "coverLightAccent", &Tokens::coverLightAccent },
    { "coverLightInk", &Tokens::coverLightInk },
    { "coverIslandChroma", &Tokens::coverIslandChroma },
    { "coverAccentChroma", &Tokens::coverAccentChroma },
    { "coverFullChroma", &Tokens::coverFullChroma },
    { "coverSecondShare", &Tokens::coverSecondShare },
    { "coverInkChroma", &Tokens::coverInkChroma },
    { "islandColor", &Tokens::islandColor, true },
    { "islandHighlightColor", &Tokens::islandHighlightColor },
    { "islandSecondColor", &Tokens::islandSecondColor }, // set from islandColor and the cover (live_tokens), not the file
    { "titleTextColor", &Tokens::titleTextColor },
    { "bodyTextColor", &Tokens::bodyTextColor },
    { "captionTextColor", &Tokens::captionTextColor },
    { "iconColor", &Tokens::iconColor },
    { "buttonColor", &Tokens::buttonColor },
    { "buttonOffColor", &Tokens::buttonOffColor },
    { "buttonDisabledColor", &Tokens::buttonDisabledColor },
    { "buttonHoverColor", &Tokens::buttonHoverColor },
    { "artHoverColor", &Tokens::artHoverColor },
    { "artHoverGlyphColor", &Tokens::artHoverGlyphColor },
    { "trackColor", &Tokens::trackColor },
    { "liveColor", &Tokens::liveColor },
    { "knobColor", &Tokens::knobColor },
    { "stepDoneColor", &Tokens::stepDoneColor },
    { "stepTodoColor", &Tokens::stepTodoColor },
    { "tabSelectedColor", &Tokens::tabSelectedColor },
    { "hoverColor", &Tokens::hoverColor },
    { "tileColor", &Tokens::tileColor },
    { "focusRingColor", &Tokens::focusRingColor },
    { "inputColor", &Tokens::inputColor },
    { "placeholderTextColor", &Tokens::placeholderTextColor },
    { "selectionColor", &Tokens::selectionColor },
    { "tagTextColor", &Tokens::tagTextColor },
    { "dangerColor", &Tokens::dangerColor },
    { "badgeColor", &Tokens::badgeColor },
    { "imagePlaceholderColor", &Tokens::imagePlaceholderColor },
    { "shadowColor", &Tokens::shadowColor },
    { "pomodoroFocusColor", &Tokens::pomodoroFocusColor },
    { "pomodoroBreakColor", &Tokens::pomodoroBreakColor },
    { "pomodoroFocusMs", &Tokens::pomodoroFocus, true },
    { "pomodoroBreakMs", &Tokens::pomodoroBreak },
    { "pomodoroFocusStepMs", &Tokens::pomodoroFocusStep },
    { "pomodoroFocusMaxMs", &Tokens::pomodoroFocusMax },
    { "pomodoroBreakStepMs", &Tokens::pomodoroBreakStep },
    { "pomodoroBreakMaxMs", &Tokens::pomodoroBreakMax },
    { "pomodoroLongBreakMs", &Tokens::pomodoroLongBreak },
    { "pomodoroRounds", &Tokens::pomodoroRounds },
    { "todoMaxLength", &Tokens::todoMaxLength },
    { "shelfCopyMaxMB", &Tokens::shelfCopyMaxMB },
    { "shelfStoreMaxMB", &Tokens::shelfStoreMaxMB },
    { "shelfMaxCheckedItems", &Tokens::shelfMaxCheckedItems },
    { "listVisibleRows", &Tokens::listVisibleRows },
    { "scrollStep", &Tokens::scrollStep },
    { "scrollDurationMs", &Tokens::scrollDuration },
    { "logsNoteMs", &Tokens::logsNote, true },
    { "updateFirstCheckDelayMs", &Tokens::updateFirstCheckDelay, true },
    { "updateCheckIntervalMs", &Tokens::updateCheckInterval },
    { "updateRetryMs", &Tokens::updateRetry },
};

const Field* findField(std::string_view name)
{
    for (const auto& f : kFields)
        if (name == f.name) return &f;
    return nullptr;
}

// --- Applying one value to one member, with shape and range checks ----------

// Returns a problem description, or empty if applied.
std::string apply(Tokens& t, const Field& field, const JsonValue& v)
{
    auto allNonNegative = [&] {
        for (float n : v.numbers) if (n < 0) return false;
        return true;
    };

    return std::visit([&](auto member) -> std::string {
        using T = std::remove_reference_t<decltype(t.*member)>;
        if constexpr (std::is_same_v<T, float>) {
            if (v.isArray) return "expected a number";
            if (!allNonNegative()) return "must not be negative";
            t.*member = v.numbers[0];
        } else if constexpr (std::is_same_v<T, Ms>) {
            if (v.isArray) return "expected a number of milliseconds";
            if (!allNonNegative()) return "must not be negative";
            t.*member = Ms{ std::lround(v.numbers[0]) };
        } else if constexpr (std::is_same_v<T, std::size_t>) {
            if (v.isArray || v.numbers[0] < 1 || v.numbers[0] != std::floor(v.numbers[0])) return "expected a whole number >= 1";
            t.*member = static_cast<std::size_t>(v.numbers[0]);
        } else if constexpr (std::is_same_v<T, Vec2>) {
            if (!v.isArray || v.numbers.size() != 2) return "expected [width, height]";
            if (!allNonNegative()) return "must not be negative";
            t.*member = { v.numbers[0], v.numbers[1] };
        } else if constexpr (std::is_same_v<T, Bezier>) {
            if (!v.isArray || v.numbers.size() != 4) return "expected [x1, y1, x2, y2]";
            if (v.numbers[0] < 0 || v.numbers[0] > 1 || v.numbers[2] < 0 || v.numbers[2] > 1) return "x1 and x2 must be within 0..1";
            t.*member = { { v.numbers[0], v.numbers[1] }, { v.numbers[2], v.numbers[3] } };
        } else if constexpr (std::is_same_v<T, Color>) {
            if (!v.isArray || (v.numbers.size() != 3 && v.numbers.size() != 4)) return "expected [r, g, b] or [r, g, b, alpha]";
            for (std::size_t i = 0; i < 3; ++i)
                if (v.numbers[i] < 0 || v.numbers[i] > 255 || v.numbers[i] != std::floor(v.numbers[i])) return "r, g, b must be whole numbers 0..255";
            float alpha = v.numbers.size() == 4 ? v.numbers[3] : 1.0f;
            if (alpha < 0 || alpha > 1) return "alpha must be within 0..1";
            auto channel = [](float n) { return static_cast<std::uint8_t>(n); };
            t.*member = { channel(v.numbers[0]), channel(v.numbers[1]), channel(v.numbers[2]),
                          static_cast<std::uint8_t>(std::lround(alpha * 255)) };
        }
        return "";
    }, field.member);
}

// [r, g, b], or [r, g, b, alpha] when it isn't opaque.
void writeColor(std::ostringstream& out, Color c)
{
    out << "[" << int(c.r) << ", " << int(c.g) << ", " << int(c.b);
    if (c.a != 255) out << ", " << c.a / 255.0;
    out << "]";
}

// "themes": { "Grape": { "islandColor": [24, 9, 42], ... }, ... } (decision 022).
// Each theme names only colour tokens: a theme recolours, it never resizes.
void readThemes(JsonReader& in, TokensParse& result)
{
    auto report = [&](int line, const std::string& where, const std::string& message) {
        result.problems.push_back("line " + std::to_string(line) + ": \"themes\" > " + where + ": " + message);
    };
    std::set<std::string> names;
    in.expect('{');
    while (!in.failed() && !in.peek('}')) {
        int line = in.line();
        Theme theme{ in.string() };
        if (theme.name.empty()) report(line, "\"\"", "a theme needs a name");
        else if (!names.insert(theme.name).second) report(line, "\"" + theme.name + "\"", "appears twice");
        in.expect(':');
        in.expect('{');
        std::set<std::string> seen;
        while (!in.failed() && !in.peek('}')) {
            int colorLine = in.line();
            std::string key = in.string();
            in.expect(':');
            JsonValue v = in.value();
            if (in.failed()) break;
            std::string where = "\"" + theme.name + "\" > \"" + key + "\"";
            const Field* field = findField(key);
            auto* color = field ? std::get_if<Color Tokens::*>(&field->member) : nullptr;
            Tokens scratch;
            if (!seen.insert(key).second) report(colorLine, where, "appears twice");
            else if (!color) report(colorLine, where, field ? "not a colour token" : "unknown token (typo?)");
            else if (std::string problem = apply(scratch, *field, v); !problem.empty()) report(colorLine, where, problem);
            else theme.colors.push_back({ key, scratch.**color });
            if (!in.peek(',')) break;
            in.expect(',');
        }
        in.expect('}');
        result.tokens.themes.push_back(std::move(theme));
        if (!in.peek(',')) break;
        in.expect(',');
    }
    in.expect('}');
}

} // namespace

TokensParse parseTokens(std::string_view json)
{
    TokensParse result;
    JsonReader in(json, result.problems);
    std::set<std::string> seen;

    in.expect('{');
    while (!in.failed() && !in.peek('}')) {
        int line = in.line();
        std::string key = in.string();
        in.expect(':');
        auto report = [&](const std::string& message) {
            result.problems.push_back("line " + std::to_string(line) + ": \"" + key + "\": " + message);
        };
        bool twice = !seen.insert(key).second;
        if (key == "themes") {
            readThemes(in, result);
            if (twice) report("appears twice");
        } else {
            JsonValue v = in.value();
            if (in.failed()) break;
            if (twice) report("appears twice");
            else if (const Field* field = findField(key)) {
                if (std::string problem = apply(result.tokens, *field, v); !problem.empty()) report(problem);
            } else report("unknown token (typo?)");
        }
        if (in.failed()) break;

        if (!in.peek(',')) break;
        in.expect(',');
    }
    in.expect('}');
    if (!in.failed() && !in.atEnd()) in.fail("unexpected text after the closing '}'");
    // What each theme looks like, for Settings' list: its island and accent,
    // and light or dark by that island (a theme that keeps the file's
    // islandColor is as light as the file). Perceived brightness, 0..255.
    for (Theme& theme : result.tokens.themes) {
        Tokens themed = withTheme(result.tokens, theme.name);
        theme.island = themed.islandColor;
        theme.accent = themed.liveColor;
        theme.light = 0.2126f * theme.island.r + 0.7152f * theme.island.g + 0.0722f * theme.island.b > 127.5f;
    }
    return result;
}

Tokens scaleTokens(const Tokens& t, float displayScale)
{
    float scale = displayScale * t.islandScale;
    // Factors, not lengths: a press squashes to 96% at any scale, and the size
    // knob isn't a size. Nor are font weights, text contrast or the cover's
    // lightnesses and chromas (decision 051).
    auto unitless = [](const char* name) {
        std::string_view n(name);
        return n.starts_with("cover") || n == "pressScale" || n == "buttonPressScale" || n == "islandScale" || n == "textContrast" || n == "listVisibleRows" || n == "listCardVisibleRows" ||
               (n.size() > 10 && n.substr(n.size() - 10) == "FontWeight");
    };
    Tokens out = t;
    for (const Field& field : kFields) {
        if (unitless(field.name)) continue;
        if (auto* f = std::get_if<float Tokens::*>(&field.member)) out.**f *= scale;
        else if (auto* v = std::get_if<Vec2 Tokens::*>(&field.member)) out.**v = { (out.**v).x * scale, (out.**v).y * scale };
    }
    return out;
}

Tokens withTheme(const Tokens& t, const std::string& name)
{
    for (const Theme& theme : t.themes)
        if (theme.name == name) return withColors(t, theme.colors);
    return t;
}

bool onlyColorsDiffer(const Tokens& a, const Tokens& b)
{
    Tokens mixed = b;
    mixed.fadeSpeed = a.fadeSpeed; // how fast colours fade isn't anything to rebuild either
    for (const Field& f : kFields)
        if (auto* member = std::get_if<Color Tokens::*>(&f.member)) mixed.*(*member) = a.*(*member);
    return mixed == a;
}

Tokens withColors(const Tokens& t, const std::vector<ThemeColor>& colors)
{
    Tokens out = t;
    for (const ThemeColor& c : colors) {
        const Field* field = findField(c.token);
        if (auto* color = field ? std::get_if<Color Tokens::*>(&field->member) : nullptr) out.**color = c.color;
    }
    return out;
}

std::string toJson(const Tokens& t)
{
    std::ostringstream out;
    out << "{\n";
    bool first = true;
    for (const auto& field : kFields) {
        if (!first) out << ",\n";
        if (!first && field.startsGroup) out << "\n";
        first = false;
        out << "  \"" << field.name << "\": ";
        std::visit([&](auto member) {
            const auto& v = t.*member;
            using T = std::remove_cvref_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Vec2>) out << "[" << v.x << ", " << v.y << "]";
            else if constexpr (std::is_same_v<T, Bezier>) out << "[" << v.c1.x << ", " << v.c1.y << ", " << v.c2.x << ", " << v.c2.y << "]";
            else if constexpr (std::is_same_v<T, Ms>) out << v.count();
            else if constexpr (std::is_same_v<T, Color>) writeColor(out, v);
            else out << v;
        }, field.member);
    }
    out << ",\n\n  \"themes\": {";
    for (std::size_t i = 0; i < t.themes.size(); ++i) {
        const Theme& theme = t.themes[i];
        out << (i ? ",\n" : "\n") << "    \"" << theme.name << "\": {";
        for (std::size_t j = 0; j < theme.colors.size(); ++j) {
            out << (j ? ", " : " ") << "\"" << theme.colors[j].token << "\": ";
            writeColor(out, theme.colors[j].color);
        }
        out << (theme.colors.empty() ? "}" : " }");
    }
    out << (t.themes.empty() ? "}" : "\n  }") << "\n}\n";
    return out.str();
}

} // namespace cup
