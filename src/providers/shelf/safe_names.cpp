#include "providers/shelf/safe_names.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace cup::shelf {
namespace {

bool isReserved(std::string stem)
{
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL") return true;
    return stem.size() == 4 && (stem.starts_with("COM") || stem.starts_with("LPT")) && stem[3] >= '1' && stem[3] <= '9';
}

// The first `maxChars` characters of UTF-8 text, a character never cut in half.
std::string firstChars(const std::string& text, std::size_t maxChars)
{
    std::size_t chars = 0, i = 0;
    for (; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) continue; // inside a character
        if (chars++ == maxChars) break;
    }
    return text.substr(0, i);
}

std::string trimmed(std::string_view text)
{
    auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!text.empty() && blank(text.front())) text.remove_prefix(1);
    while (!text.empty() && blank(text.back())) text.remove_suffix(1);
    return std::string(text);
}

} // namespace

std::string safeFileName(const std::string& name, std::size_t maxChars)
{
    std::string out;
    for (char c : name) {
        unsigned char u = static_cast<unsigned char>(c);
        bool bad = u < 32 || u == 127 || std::string_view("\\/:*?\"<>|").find(c) != std::string_view::npos;
        out += bad ? '_' : c;
    }
    out = firstChars(trimmed(out), maxChars);
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back(); // Windows drops them, then can't find the file
    if (out.empty()) return "Untitled";
    if (isReserved(out.substr(0, out.find('.')))) out = "_" + out; // "con.txt" is the console, not a file
    return out;
}

std::string textFileName(const std::string& text)
{
    std::string first;
    for (std::size_t from = 0; from <= text.size() && first.empty();) {
        std::size_t end = text.find('\n', from);
        first = trimmed(std::string_view(text).substr(from, end == std::string::npos ? std::string::npos : end - from));
        if (end == std::string::npos) break;
        from = end + 1;
    }
    // A few words: cut at the last space before 40 characters.
    std::string cut = firstChars(first, 40);
    if (cut.size() < first.size() && cut.rfind(' ') != std::string::npos && cut.rfind(' ') > 10) cut.erase(cut.rfind(' '));
    return safeFileName(cut, 40) + ".txt";
}

std::string textFileBody(const std::string& text) { return "\xEF\xBB\xBF" + text; }

std::string urlFileName(const std::string& url, const std::string& title)
{
    std::string name = trimmed(title);
    if (name.empty()) {
        std::size_t start = url.find("://");
        start = start == std::string::npos ? 0 : start + 3;
        std::size_t end = url.find_first_of("/?#", start);
        name = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (auto at = name.rfind('@'); at != std::string::npos) name.erase(0, at + 1); // no user:password in a file name
        if (name.starts_with("www.")) name.erase(0, 4);
    }
    return safeFileName(name, 60) + ".url";
}

std::string urlFileBody(const std::string& url)
{
    std::string oneLine;
    for (char c : url)
        if (c != '\r' && c != '\n') oneLine += c;
    return "[InternetShortcut]\r\nURL=" + oneLine + "\r\n";
}

} // namespace cup::shelf
