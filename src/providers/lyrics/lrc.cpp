#include "providers/lyrics/lrc.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>

namespace cup {
namespace {

std::string_view trimmed(std::string_view s)
{
    auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    while (!s.empty() && blank(s.front())) s.remove_prefix(1);
    while (!s.empty() && blank(s.back())) s.remove_suffix(1);
    return s;
}

bool digits(std::string_view s) { return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; }); }

// "mm:ss", "mm:ss.xx", "mm:ss.xxx" or "mm:ss:xx" (some files) -> ms. Anything else isn't a time.
std::optional<Ms> timeOf(std::string_view tag)
{
    auto colon = tag.find(':');
    if (colon == std::string_view::npos) return std::nullopt;
    std::string_view minutes = tag.substr(0, colon), rest = tag.substr(colon + 1);
    auto dot = rest.find_first_of(".:");
    std::string_view seconds = rest.substr(0, dot), fraction = dot == std::string_view::npos ? std::string_view() : rest.substr(dot + 1);
    if (!digits(minutes) || !digits(seconds) || minutes.size() > 4 || seconds.size() > 2) return std::nullopt;
    if (!fraction.empty() && (!digits(fraction) || fraction.size() > 3)) return std::nullopt;
    long long ms = std::atoll(std::string(minutes).c_str()) * 60000 + std::atoll(std::string(seconds).c_str()) * 1000;
    if (!fraction.empty()) { // ".5" is half a second, ".50" and ".500" too
        long long part = std::atoll(std::string(fraction).c_str());
        for (std::size_t i = fraction.size(); i < 3; ++i) part *= 10;
        ms += part;
    }
    return Ms{ ms };
}

} // namespace

std::vector<LyricLine> parseLrc(std::string_view text)
{
    std::vector<LyricLine> lines;
    Ms offset{ 0 };
    while (!text.empty()) {
        auto end = text.find('\n');
        std::string_view line = trimmed(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);

        std::vector<Ms> times;
        while (!line.empty() && line.front() == '[') {
            auto close = line.find(']');
            if (close == std::string_view::npos) break;
            std::string_view tag = trimmed(line.substr(1, close - 1));
            if (auto at = timeOf(tag)) times.push_back(*at);
            else if (tag.rfind("offset:", 0) == 0) offset = Ms{ std::atoll(std::string(trimmed(tag.substr(7))).c_str()) };
            else if (!times.empty()) break; // "[00:10.00][chorus] …": the rest is the line's words
            line = trimmed(line.substr(close + 1));
        }
        for (Ms at : times) lines.push_back({ at, std::string(line) });
    }
    for (LyricLine& line : lines) line.at = std::max(Ms{ 0 }, line.at - offset);
    std::stable_sort(lines.begin(), lines.end(), [](const LyricLine& a, const LyricLine& b) { return a.at < b.at; });
    return lines;
}

} // namespace cup
