#include "core/tokens_json.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>
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
    { "idleSize", &Tokens::idleSize, true },
    { "compactSize", &Tokens::compactSize },
    { "alertSize", &Tokens::alertSize },
    { "expandedSize", &Tokens::expandedSize },
    { "receptiveSize", &Tokens::receptiveSize },
    { "hiddenSize", &Tokens::hiddenSize },
    { "maxCornerRadius", &Tokens::maxCornerRadius },
    { "compactInset", &Tokens::compactInset, true },
    { "alertPadding", &Tokens::alertPadding },
    { "expandedPadding", &Tokens::expandedPadding },
    { "expandedArtSize", &Tokens::expandedArtSize },
    { "slotGap", &Tokens::slotGap },
    { "rowSpacing", &Tokens::rowSpacing },
    { "itemSpacing", &Tokens::itemSpacing },
    { "looseItemSpacing", &Tokens::looseItemSpacing },
    { "titleFontSize", &Tokens::titleFontSize },
    { "bodyFontSize", &Tokens::bodyFontSize },
    { "captionFontSize", &Tokens::captionFontSize },
    { "displayFontSize", &Tokens::displayFontSize },
    { "iconSize", &Tokens::iconSize },
    { "buttonSize", &Tokens::buttonSize },
    { "buttonGlyphSize", &Tokens::buttonGlyphSize },
    { "buttonPressScale", &Tokens::buttonPressScale },
    { "progressHeight", &Tokens::progressHeight },
    { "progressHitHeight", &Tokens::progressHitHeight },
    { "progressKnobSize", &Tokens::progressKnobSize },
    { "timeLabelWidth", &Tokens::timeLabelWidth },
    { "ringThickness", &Tokens::ringThickness },
    { "stepDotSize", &Tokens::stepDotSize },
    { "stepDotGap", &Tokens::stepDotGap },
    { "hoverFadeDurationMs", &Tokens::hoverFadeDuration, true },
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
    { "tabHeight", &Tokens::tabHeight, true },
    { "tabGap", &Tokens::tabGap },
    { "tabMaxWidth", &Tokens::tabMaxWidth },
    { "homeButtonWidth", &Tokens::homeButtonWidth },
    { "homeColumns", &Tokens::homeColumns, true },
    { "tileHeight", &Tokens::tileHeight },
    { "tileGap", &Tokens::tileGap },
    { "tilePadding", &Tokens::tilePadding },
    { "morphDurationMs", &Tokens::morphDuration, true },
    { "morphEase", &Tokens::morphEase },
    { "contentFadeDurationMs", &Tokens::contentFadeDuration },
    { "contentFadeDelayMs", &Tokens::contentFadeDelay },
    { "pressScale", &Tokens::pressScale, true },
    { "pressDownDurationMs", &Tokens::pressDownDuration },
    { "pressDownEase", &Tokens::pressDownEase },
    { "pressReleaseDurationMs", &Tokens::pressReleaseDuration },
    { "pressReleaseEase", &Tokens::pressReleaseEase },
    { "alertDurationMs", &Tokens::alertDuration, true },
    { "expandedIdleTimeoutMs", &Tokens::expandedIdleTimeout },
    { "heldAlertMaxAgeMs", &Tokens::heldAlertMaxAge },
    { "alertQueueCapacity", &Tokens::alertQueueCapacity },
    { "mediaPausedLingerMs", &Tokens::mediaPausedLinger, true },
    { "mediaSessionRecheckMs", &Tokens::mediaSessionRecheck },
    { "islandColor", &Tokens::islandColor, true },
    { "islandHighlightColor", &Tokens::islandHighlightColor },
    { "titleTextColor", &Tokens::titleTextColor },
    { "bodyTextColor", &Tokens::bodyTextColor },
    { "captionTextColor", &Tokens::captionTextColor },
    { "iconColor", &Tokens::iconColor },
    { "buttonColor", &Tokens::buttonColor },
    { "buttonOffColor", &Tokens::buttonOffColor },
    { "buttonDisabledColor", &Tokens::buttonDisabledColor },
    { "buttonHoverColor", &Tokens::buttonHoverColor },
    { "trackColor", &Tokens::trackColor },
    { "liveColor", &Tokens::liveColor },
    { "knobColor", &Tokens::knobColor },
    { "stepDoneColor", &Tokens::stepDoneColor },
    { "stepTodoColor", &Tokens::stepTodoColor },
    { "tabSelectedColor", &Tokens::tabSelectedColor },
    { "hoverColor", &Tokens::hoverColor },
    { "tileColor", &Tokens::tileColor },
    { "imagePlaceholderColor", &Tokens::imagePlaceholderColor },
    { "pomodoroFocusColor", &Tokens::pomodoroFocusColor },
    { "pomodoroBreakColor", &Tokens::pomodoroBreakColor },
    { "pomodoroFocusMs", &Tokens::pomodoroFocus, true },
    { "pomodoroBreakMs", &Tokens::pomodoroBreak },
    { "pomodoroLongBreakMs", &Tokens::pomodoroLongBreak },
    { "pomodoroRounds", &Tokens::pomodoroRounds },
};

const Field* findField(const std::string& name)
{
    for (const auto& f : kFields)
        if (name == f.name) return &f;
    return nullptr;
}

// --- Minimal JSON reader: one flat object of numbers / number arrays -------

struct Value {
    std::vector<float> numbers;
    bool isArray = false;
};

class Reader {
public:
    Reader(std::string_view text, std::vector<std::string>& problems) : text_(text), problems_(problems) {}

    int line() const { return line_; }
    bool failed() const { return failed_; }

    void fail(const std::string& message)
    {
        if (!failed_) problems_.push_back("line " + std::to_string(line_) + ": " + message);
        failed_ = true;
    }

    void skipSpace()
    {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            if (text_[pos_] == '\n') ++line_;
            ++pos_;
        }
    }

    bool peek(char c) { skipSpace(); return pos_ < text_.size() && text_[pos_] == c; }
    bool atEnd() { skipSpace(); return pos_ >= text_.size(); }

    bool expect(char c)
    {
        if (peek(c)) { ++pos_; return true; }
        fail(std::string("expected '") + c + "'");
        return false;
    }

    std::string string()
    {
        std::string out;
        if (!expect('"')) return out;
        while (pos_ < text_.size() && text_[pos_] != '"') {
            if (text_[pos_] == '\\' && pos_ + 1 < text_.size()) ++pos_;
            if (text_[pos_] == '\n') { fail("unterminated string"); return out; }
            out += text_[pos_++];
        }
        expect('"');
        return out;
    }

    float number()
    {
        skipSpace();
        size_t start = pos_;
        while (pos_ < text_.size() && std::string_view("+-.0123456789eE").find(text_[pos_]) != std::string_view::npos) ++pos_;
        std::string digits(text_.substr(start, pos_ - start));
        char* end = nullptr;
        double value = std::strtod(digits.c_str(), &end);
        if (digits.empty() || end != digits.c_str() + digits.size() || !std::isfinite(value)) {
            fail("expected a number");
            return 0;
        }
        return static_cast<float>(value);
    }

    Value value()
    {
        Value v;
        if (!peek('[')) {
            v.numbers.push_back(number());
            return v;
        }
        ++pos_;
        v.isArray = true;
        if (peek(']')) { ++pos_; return v; }
        while (true) {
            v.numbers.push_back(number());
            if (failed_ || !peek(',')) break;
            ++pos_;
        }
        expect(']');
        return v;
    }

private:
    std::string_view text_;
    std::vector<std::string>& problems_;
    size_t pos_ = 0;
    int line_ = 1;
    bool failed_ = false;
};

// --- Applying one value to one member, with shape and range checks ----------

// Returns a problem description, or empty if applied.
std::string apply(Tokens& t, const Field& field, const Value& v)
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

} // namespace

TokensParse parseTokens(std::string_view json)
{
    TokensParse result;
    Reader in(json, result.problems);
    std::set<std::string> seen;

    in.expect('{');
    while (!in.failed() && !in.peek('}')) {
        int line = in.line();
        std::string key = in.string();
        in.expect(':');
        Value v = in.value();
        if (in.failed()) break;

        auto report = [&](const std::string& message) {
            result.problems.push_back("line " + std::to_string(line) + ": \"" + key + "\": " + message);
        };
        if (!seen.insert(key).second) report("appears twice");
        else if (const Field* field = findField(key)) {
            if (std::string problem = apply(result.tokens, *field, v); !problem.empty()) report(problem);
        } else report("unknown token (typo?)");

        if (!in.peek(',')) break;
        in.expect(',');
    }
    in.expect('}');
    if (!in.failed() && !in.atEnd()) in.fail("unexpected text after the closing '}'");
    return result;
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
            else if constexpr (std::is_same_v<T, Color>) {
                out << "[" << int(v.r) << ", " << int(v.g) << ", " << int(v.b);
                if (v.a != 255) out << ", " << v.a / 255.0;
                out << "]";
            }
            else out << v;
        }, field.member);
    }
    out << "\n}\n";
    return out.str();
}

} // namespace cup
