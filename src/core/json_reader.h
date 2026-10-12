#pragma once
// The small JSON reader behind tokens.json: objects, strings, numbers and
// number arrays, which is all the file uses. Reports the first syntax error
// with its line number. Pure.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace cup {

// A number, or an array of numbers ([w, h], a curve, a colour).
struct JsonValue {
    std::vector<float> numbers;
    bool isArray = false;
};

class JsonReader {
public:
    JsonReader(std::string_view text, std::vector<std::string>& problems) : text_(text), problems_(problems) {}

    int line() const { return line_; }
    bool failed() const { return failed_; }
    void fail(const std::string& message); // records "line n: message", once

    bool peek(char c); // the next non-space character is c
    bool atEnd();      // only spaces left
    bool expect(char c);
    std::string string();
    float number();
    JsonValue value(); // a number or an array of numbers

private:
    void skipSpace();

    std::string_view text_;
    std::vector<std::string>& problems_;
    std::size_t pos_ = 0;
    int line_ = 1;
    bool failed_ = false;
};

} // namespace cup
