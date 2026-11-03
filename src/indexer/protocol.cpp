#include "indexer/protocol.h"

#include "indexer/json.h"

#include <cmath>
#include <string>

namespace cup::indexer {
namespace {

// A JSON number that is a whole number a double holds exactly.
std::optional<std::int64_t> wholeNumber(const Json* v)
{
    if (!v || !v->isNumber() || v->number != std::floor(v->number) || std::fabs(v->number) > 9007199254740992.0)
        return std::nullopt;
    return static_cast<std::int64_t>(v->number);
}

std::string opening(std::optional<std::int64_t> id)
{
    std::string out = "{\"jsonrpc\":\"2.0\",\"id\":";
    out += id ? std::to_string(*id) : "null";
    return out;
}

const char* stateName(VolumeState s)
{
    switch (s) {
    case VolumeState::Scanning: return "scanning";
    case VolumeState::Ready: return "ready";
    case VolumeState::Failed: return "failed";
    }
    return "failed";
}

std::optional<VolumeState> stateFrom(std::string_view name)
{
    if (name == "scanning") return VolumeState::Scanning;
    if (name == "ready") return VolumeState::Ready;
    if (name == "failed") return VolumeState::Failed;
    return std::nullopt;
}

bool flagOf(const Json& object, std::string_view key)
{
    const Json* v = object.get(key);
    return v && v->type == Json::Type::Bool && v->flag;
}

} // namespace

std::string frame(std::string_view body)
{
    std::string out;
    auto length = static_cast<std::uint32_t>(body.size());
    for (int i = 0; i < 4; ++i) out += static_cast<char>((length >> (8 * i)) & 0xFF);
    out += body;
    return out;
}

void FrameReader::feed(std::span<const std::uint8_t> bytes)
{
    if (!broken_) pending_.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::optional<std::string> FrameReader::next()
{
    if (broken_ || pending_.size() < 4) return std::nullopt;
    std::uint32_t length = 0;
    for (int i = 0; i < 4; ++i) length |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(pending_[i])) << (8 * i);
    if (length > max_) {
        broken_ = true;
        pending_.clear();
        return std::nullopt;
    }
    if (pending_.size() - 4 < length) return std::nullopt;
    std::string body = pending_.substr(4, length);
    pending_.erase(0, 4 + static_cast<std::size_t>(length));
    return body;
}

Request parseRequest(std::string_view body)
{
    if (body.size() > kMaxRequestBytes) return BadRequest{ std::nullopt, kInvalidRequest, "request too long" };
    auto json = parseJson(body);
    if (!json) return BadRequest{ std::nullopt, kParseError, "not JSON" };
    if (json->type != Json::Type::Object) return BadRequest{ std::nullopt, kInvalidRequest, "not an object" };
    auto id = wholeNumber(json->get("id"));
    const Json* version = json->get("jsonrpc");
    const Json* method = json->get("method");
    if (!id) return BadRequest{ std::nullopt, kInvalidRequest, "a whole-number id is needed" };
    if (!version || !version->isString() || version->text != "2.0") return BadRequest{ id, kInvalidRequest, "jsonrpc must be \"2.0\"" };
    if (!method || !method->isString()) return BadRequest{ id, kInvalidRequest, "no method" };
    const Json* params = json->get("params");
    if (params && params->type != Json::Type::Object) return BadRequest{ id, kInvalidParams, "params must be an object" };

    if (method->text == "status") return StatusRequest{ *id };
    if (method->text != "query") return BadRequest{ id, kMethodNotFound, "unknown method" };
    const Json* text = params ? params->get("text") : nullptr;
    if (!text || !text->isString()) return BadRequest{ id, kInvalidParams, "query needs text" };
    QueryRequest q{ *id, text->text, kDefaultResults };
    if (const Json* limit = params->get("limit")) {
        auto n = wholeNumber(limit);
        if (!n || *n < 1) return BadRequest{ id, kInvalidParams, "limit must be a whole number from 1" };
        q.limit = *n > static_cast<std::int64_t>(kMaxResults) ? kMaxResults : static_cast<std::size_t>(*n);
    }
    return q;
}

std::string encodeQuery(std::int64_t id, std::string_view text, std::size_t limit)
{
    std::string out = opening(id) + ",\"method\":\"query\",\"params\":{\"text\":";
    appendJsonString(out, text);
    out += ",\"limit\":" + std::to_string(limit) + "}}";
    return out;
}

std::string encodeStatusRequest(std::int64_t id)
{
    return opening(id) + ",\"method\":\"status\"}";
}

std::string encodeResults(std::int64_t id, const std::vector<ResultRow>& rows)
{
    std::string out = opening(id) + ",\"result\":{\"hits\":[";
    const std::string closing = "]}}";
    bool first = true;
    for (const ResultRow& row : rows) {
        std::string item = first ? "{\"name\":" : ",{\"name\":";
        appendJsonString(item, row.name);
        item += ",\"path\":";
        appendJsonString(item, row.path);
        item += row.folder ? ",\"folder\":true" : ",\"folder\":false";
        item += row.hidden ? ",\"hidden\":true}" : ",\"hidden\":false}";
        if (out.size() + item.size() + closing.size() > kMaxResponseBytes) break;
        out += item;
        first = false;
    }
    return out + closing;
}

std::string encodeStatus(std::int64_t id, const std::vector<VolumeStatus>& volumes)
{
    std::string out = opening(id) + ",\"result\":{\"protocol\":" + std::to_string(kProtocolVersion) + ",\"volumes\":[";
    for (std::size_t i = 0; i < volumes.size(); ++i) {
        out += i ? ",{\"root\":" : "{\"root\":";
        appendJsonString(out, volumes[i].root);
        out += ",\"state\":\"";
        out += stateName(volumes[i].state);
        out += "\",\"files\":" + std::to_string(volumes[i].files) + "}";
    }
    return out + "]}}";
}

std::string encodeError(std::optional<std::int64_t> id, int code, std::string_view message)
{
    std::string out = opening(id) + ",\"error\":{\"code\":" + std::to_string(code) + ",\"message\":";
    appendJsonString(out, message);
    return out + "}}";
}

std::optional<Response> parseResponse(std::string_view body)
{
    auto json = parseJson(body);
    if (!json || json->type != Json::Type::Object) return std::nullopt;
    Response r;
    auto id = wholeNumber(json->get("id"));
    r.id = id.value_or(0);
    if (const Json* error = json->get("error")) {
        const Json* message = error->get("message");
        r.error = message && message->isString() ? message->text : "error";
        return r;
    }
    const Json* result = json->get("result");
    if (!id || !result || result->type != Json::Type::Object) return std::nullopt;
    if (const Json* hits = result->get("hits")) {
        if (hits->type != Json::Type::Array) return std::nullopt;
        std::vector<ResultRow> rows;
        for (const Json& h : hits->items) {
            const Json* name = h.get("name");
            const Json* path = h.get("path");
            if (!name || !name->isString() || !path || !path->isString()) return std::nullopt;
            rows.push_back({ name->text, path->text, flagOf(h, "folder"), flagOf(h, "hidden") });
        }
        r.rows = std::move(rows);
        return r;
    }
    const Json* volumes = result->get("volumes");
    if (!volumes || volumes->type != Json::Type::Array) return std::nullopt;
    r.protocol = static_cast<int>(wholeNumber(result->get("protocol")).value_or(0));
    std::vector<VolumeStatus> list;
    for (const Json& v : volumes->items) {
        const Json* root = v.get("root");
        const Json* state = v.get("state");
        auto files = wholeNumber(v.get("files"));
        auto s = state && state->isString() ? stateFrom(state->text) : std::nullopt;
        if (!root || !root->isString() || !s || !files || *files < 0) return std::nullopt;
        list.push_back({ root->text, *s, static_cast<std::size_t>(*files) });
    }
    r.volumes = std::move(list);
    return r;
}

} // namespace cup::indexer
