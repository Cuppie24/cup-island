#include "indexer/json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace cup::indexer {
namespace {

void appendUtf8(std::string& out, unsigned c)
{
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xC0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xE0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    }
}

struct Parser {
    std::string_view s;
    std::size_t at = 0;

    void space()
    {
        while (at < s.size() && (s[at] == ' ' || s[at] == '\t' || s[at] == '\n' || s[at] == '\r')) ++at;
    }

    bool literal(std::string_view word)
    {
        if (s.substr(at, word.size()) != word) return false;
        at += word.size();
        return true;
    }

    bool hex4(unsigned& out)
    {
        if (s.size() - at < 4) return false;
        out = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s[at++];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<unsigned>(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    bool string(std::string& out)
    {
        if (at >= s.size() || s[at] != '"') return false;
        ++at;
        while (at < s.size()) {
            char c = s[at++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false; // raw control characters aren't JSON
            if (c != '\\') {
                out += c;
                continue;
            }
            if (at >= s.size()) return false;
            char e = s[at++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned c1 = 0;
                if (!hex4(c1)) return false;
                if (c1 >= 0xDC00 && c1 <= 0xDFFF) return false;
                if (c1 >= 0xD800 && c1 <= 0xDBFF) {
                    unsigned c2 = 0;
                    if (!literal("\\u") || !hex4(c2) || c2 < 0xDC00 || c2 > 0xDFFF) return false;
                    c1 = 0x10000 + ((c1 - 0xD800) << 10) + (c2 - 0xDC00);
                }
                appendUtf8(out, c1);
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    bool number(double& out)
    {
        std::size_t start = at;
        if (at < s.size() && s[at] == '-') ++at;
        while (at < s.size() && std::string_view("0123456789.eE+-").find(s[at]) != std::string_view::npos) ++at;
        std::string digits(s.substr(start, at - start));
        if (digits.empty() || digits == "-") return false;
        char* end = nullptr;
        out = std::strtod(digits.c_str(), &end);
        return end == digits.c_str() + digits.size() && std::isfinite(out);
    }

    bool value(Json& v, int depth)
    {
        if (depth > kMaxJsonDepth) return false;
        space();
        if (at >= s.size()) return false;
        char c = s[at];
        if (c == '{') {
            v.type = Json::Type::Object;
            ++at;
            space();
            if (at < s.size() && s[at] == '}') {
                ++at;
                return true;
            }
            for (;;) {
                space();
                std::pair<std::string, Json> m;
                if (!string(m.first)) return false;
                space();
                if (at >= s.size() || s[at++] != ':') return false;
                if (!value(m.second, depth + 1)) return false;
                v.members.push_back(std::move(m));
                space();
                if (at >= s.size()) return false;
                if (s[at] == '}') {
                    ++at;
                    return true;
                }
                if (s[at++] != ',') return false;
            }
        }
        if (c == '[') {
            v.type = Json::Type::Array;
            ++at;
            space();
            if (at < s.size() && s[at] == ']') {
                ++at;
                return true;
            }
            for (;;) {
                Json item;
                if (!value(item, depth + 1)) return false;
                v.items.push_back(std::move(item));
                space();
                if (at >= s.size()) return false;
                if (s[at] == ']') {
                    ++at;
                    return true;
                }
                if (s[at++] != ',') return false;
            }
        }
        if (c == '"') {
            v.type = Json::Type::String;
            return string(v.text);
        }
        if (literal("true")) {
            v.type = Json::Type::Bool;
            v.flag = true;
            return true;
        }
        if (literal("false")) {
            v.type = Json::Type::Bool;
            return true;
        }
        if (literal("null")) return true;
        v.type = Json::Type::Number;
        return number(v.number);
    }
};

} // namespace

const Json* Json::get(std::string_view key) const
{
    for (const auto& [name, value] : members)
        if (name == key) return &value;
    return nullptr;
}

std::optional<Json> parseJson(std::string_view text)
{
    Parser p{ text };
    Json v;
    if (!p.value(v, 1)) return std::nullopt;
    p.space();
    if (p.at != text.size()) return std::nullopt;
    return v;
}

void appendJsonString(std::string& out, std::string_view text)
{
    out += '"';
    for (char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char escaped[8];
                std::snprintf(escaped, sizeof escaped, "\\u%04x", static_cast<unsigned>(c));
                out += escaped;
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

} // namespace cup::indexer
