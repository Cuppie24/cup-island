#include "core/json_reader.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace cup {

void JsonReader::fail(const std::string& message)
{
    if (!failed_) problems_.push_back("line " + std::to_string(line_) + ": " + message);
    failed_ = true;
}

void JsonReader::skipSpace()
{
    while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
        if (text_[pos_] == '\n') ++line_;
        ++pos_;
    }
}

bool JsonReader::peek(char c) { skipSpace(); return pos_ < text_.size() && text_[pos_] == c; }
bool JsonReader::atEnd() { skipSpace(); return pos_ >= text_.size(); }

bool JsonReader::expect(char c)
{
    if (peek(c)) { ++pos_; return true; }
    fail(std::string("expected '") + c + "'");
    return false;
}

std::string JsonReader::string()
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

float JsonReader::number()
{
    skipSpace();
    std::size_t start = pos_;
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

JsonValue JsonReader::value()
{
    JsonValue v;
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

} // namespace cup
