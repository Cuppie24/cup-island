#pragma once
// A small JSON reader and string writer for the index service's pipe
// (decision 075). core's JsonReader is shaped for tokens.json (floats, no
// \u escapes, no nesting beyond arrays of numbers); the pipe needs objects,
// arrays, \u escapes and a hard depth limit, since what arrives on it is
// untrusted input to a SYSTEM process.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cup::indexer {

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool flag = false;
    double number = 0;
    std::string text;                                 // String (UTF-8, unescaped)
    std::vector<Json> items;                          // Array
    std::vector<std::pair<std::string, Json>> members; // Object, in order

    const Json* get(std::string_view key) const; // an Object's member, or null
    bool isString() const { return type == Type::String; }
    bool isNumber() const { return type == Type::Number; }
};

// Nesting deeper than this is refused: a request is a few levels deep.
constexpr int kMaxJsonDepth = 8;

// The whole text must be one value (spaces around allowed). Anything else,
// including a lone surrogate in a \u escape, is refused.
std::optional<Json> parseJson(std::string_view text);

// Appends `text` (UTF-8) as a JSON string, quotes included.
void appendJsonString(std::string& out, std::string_view text);

} // namespace cup::indexer
