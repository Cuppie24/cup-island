#include "core/text_edit.h"

#include <algorithm>

namespace cup {
namespace {

bool continuation(const std::string& s, std::size_t i) { return i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80; }

// A word is letters and digits; anything outside ASCII counts as a letter (é, ж, 中).
bool wordChar(const std::string& s, std::size_t i)
{
    unsigned char c = static_cast<unsigned char>(s[i]);
    return c >= 0x80 || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool space(const std::string& s, std::size_t i) { return s[i] == ' ' || s[i] == '\t'; }

std::size_t snap(const std::string& s, std::size_t at)
{
    at = std::min(at, s.size());
    while (at > 0 && continuation(s, at)) --at;
    return at;
}

std::size_t prevChar(const std::string& s, std::size_t at) { return at == 0 ? 0 : snap(s, at - 1); }
std::size_t nextChar(const std::string& s, std::size_t at)
{
    if (at >= s.size()) return s.size();
    ++at;
    while (continuation(s, at)) ++at;
    return at;
}

// Ctrl+Left: back over spaces, then over the word (or the punctuation) before them.
std::size_t prevWord(const std::string& s, std::size_t at)
{
    while (at > 0 && space(s, prevChar(s, at))) at = prevChar(s, at);
    if (at == 0) return 0;
    bool w = wordChar(s, prevChar(s, at));
    while (at > 0 && !space(s, prevChar(s, at)) && wordChar(s, prevChar(s, at)) == w) at = prevChar(s, at);
    return at;
}

// Ctrl+Right: over the word, then the spaces after it, like Windows: the caret lands on the next word's start.
std::size_t nextWord(const std::string& s, std::size_t at)
{
    if (at < s.size() && !space(s, at)) {
        bool w = wordChar(s, at);
        while (at < s.size() && !space(s, at) && wordChar(s, at) == w) at = nextChar(s, at);
    }
    while (at < s.size() && space(s, at)) at = nextChar(s, at);
    return at;
}

bool hasSelection(const Draft& d) { return d.caret != d.anchor; }

void eraseSelection(Draft& d)
{
    std::size_t from = std::min(d.caret, d.anchor), to = std::max(d.caret, d.anchor);
    d.text.erase(from, to - from);
    d.caret = d.anchor = from;
}

// Offsets from before an edit made outside this file (a provider, a test) stay valid.
void sanitize(Draft& d)
{
    d.caret = snap(d.text, d.caret);
    d.anchor = snap(d.text, d.anchor);
}

} // namespace

void insertAtCaret(Draft& d, const std::string& text)
{
    sanitize(d);
    eraseSelection(d);
    d.text.insert(d.caret, text);
    d.caret = d.anchor = d.caret + text.size();
}

void eraseAtCaret(Draft& d, bool forward, bool word)
{
    sanitize(d);
    if (hasSelection(d)) return eraseSelection(d);
    std::size_t other = forward ? (word ? nextWord(d.text, d.caret) : nextChar(d.text, d.caret))
                                : (word ? prevWord(d.text, d.caret) : prevChar(d.text, d.caret));
    d.anchor = other;
    eraseSelection(d);
}

void moveCaret(Draft& d, CaretMove move, bool select)
{
    sanitize(d);
    std::size_t from = std::min(d.caret, d.anchor), to = std::max(d.caret, d.anchor);
    std::size_t at = d.caret;
    switch (move) {
    // A selection without Shift: the arrow lands on its edge, like any text box.
    case CaretMove::Left: at = !select && hasSelection(d) ? from : prevChar(d.text, d.caret); break;
    case CaretMove::Right: at = !select && hasSelection(d) ? to : nextChar(d.text, d.caret); break;
    case CaretMove::WordLeft: at = prevWord(d.text, d.caret); break;
    case CaretMove::WordRight: at = nextWord(d.text, d.caret); break;
    case CaretMove::Start: at = 0; break;
    case CaretMove::End: at = d.text.size(); break;
    }
    d.caret = at;
    if (!select) d.anchor = at;
}

void placeCaret(Draft& d, std::size_t at, bool select)
{
    sanitize(d);
    d.caret = snap(d.text, at);
    if (!select) d.anchor = d.caret;
}

void selectWordAt(Draft& d, std::size_t at)
{
    const std::string& s = d.text;
    at = snap(s, at);
    if (s.empty()) return placeCaret(d, 0, false);
    if (at == s.size()) at = prevChar(s, at); // past the end: the last word
    std::size_t from = at, to = nextChar(s, at);
    auto same = [&](std::size_t i) { return space(s, i) == space(s, at) && (space(s, at) || wordChar(s, i) == wordChar(s, at)); };
    while (from > 0 && same(prevChar(s, from))) from = prevChar(s, from);
    while (to < s.size() && same(to)) to = nextChar(s, to);
    d.anchor = from;
    d.caret = to;
}

void selectAll(Draft& d)
{
    d.anchor = 0;
    d.caret = d.text.size();
}

std::string selectedText(const Draft& d)
{
    std::size_t from = std::min({ d.caret, d.anchor, d.text.size() }), to = std::min(std::max(d.caret, d.anchor), d.text.size());
    return d.text.substr(from, to - from);
}

// A lead byte 11110xxx starts a character outside the BMP: two UTF-16 units (a surrogate pair).
std::size_t utf16Index(std::string_view s, std::size_t bytes)
{
    std::size_t units = 0;
    for (std::size_t i = 0; i < std::min(bytes, s.size()); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c & 0xC0) != 0x80) units += c >= 0xF0 ? 2 : 1;
    }
    return units;
}

std::size_t utf8Offset(std::string_view s, std::size_t utf16)
{
    std::size_t units = 0, i = 0;
    while (i < s.size() && units < utf16) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        units += c >= 0xF0 ? 2 : 1;
        ++i;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i;
    }
    return i;
}

} // namespace cup
