// The index service's pipe (decision 075): JSON, framing, requests and
// answers both ways, who sees which results, and merging drives' results.

#include "expect.h"
#include "indexer/folder_access.h"
#include "indexer/json.h"
#include "indexer/protocol.h"
#include "indexer/query.h"

#include <random>

namespace cup::test {

using namespace cup::indexer;

namespace {

void jsonTests()
{
    auto v = parseJson(R"( {"a": [1, -2.5e1, true, false, null], "b": {"c": "x\"\\\/\n\u00e9\ud83c\udfb5"}} )");
    expect(v && v->type == Json::Type::Object && v->members.size() == 2, "an object with members in order");
    const Json* a = v ? v->get("a") : nullptr;
    expect(a && a->items.size() == 5 && a->items[1].number == -25 && a->items[2].flag && !a->items[3].flag &&
           a->items[4].type == Json::Type::Null, "numbers, booleans and null in an array");
    const Json* c = v ? v->get("b")->get("c") : nullptr;
    expect(c && c->text == "x\"\\/\n\xC3\xA9\xF0\x9F\x8E\xB5", "escapes, \\u and surrogate pairs to UTF-8");
    expect(!parseJson("{\"a\":1,}") && !parseJson("[1 2]") && !parseJson("{\"a\" 1}") && !parseJson("") &&
           !parseJson("01x") && !parseJson("\"a") && !parseJson("{} {}"), "malformed JSON is refused");
    expect(!parseJson("\"\\ud800\"") && !parseJson("\"\\udc00\"") && !parseJson("\"\\x\""), "lone surrogates and bad escapes");
    expect(!parseJson("\"a\tb\""), "raw control characters in strings");
    expect(parseJson("[[[[[[[1]]]]]]]") && !parseJson("[[[[[[[[1]]]]]]]]"), "nesting is capped at kMaxJsonDepth");
    std::string out;
    appendJsonString(out, "a\"b\\c\nd\x01" "é");
    expect(out == "\"a\\\"b\\\\c\\nd\\u0001é\"", "strings are escaped on the way out");
    expect(parseJson(out)->text == "a\"b\\c\nd\x01" "é", "and read back the same");

    // Random bytes never crash the parser.
    std::mt19937 random(75);
    for (int i = 0; i < 3000; ++i) {
        std::string junk;
        std::size_t length = random() % 40;
        for (std::size_t j = 0; j < length; ++j) junk += "{}[]\",:\\u0123456789eE.-+truefalsnl \x80"[random() % 39];
        (void)parseJson(junk);
    }
    expect(true, "survived random JSON");
}

void framingTests()
{
    std::string framed = frame("hello") + frame("") + frame("world");
    FrameReader reader(kMaxRequestBytes);
    // Byte by byte: messages come out whole, however the pipe splits them.
    std::vector<std::string> got;
    for (char c : framed) {
        auto byte = static_cast<std::uint8_t>(c);
        reader.feed(std::span<const std::uint8_t>(&byte, 1));
        while (auto m = reader.next()) got.push_back(*m);
    }
    expect(got == std::vector<std::string>{ "hello", "", "world" }, "frames come out whole and in order");

    FrameReader small(4);
    std::string big = frame("12345");
    small.feed(std::span(reinterpret_cast<const std::uint8_t*>(big.data()), big.size()));
    expect(!small.next() && small.broken(), "a frame over the limit breaks the reader");
    std::string after = frame("ok");
    small.feed(std::span(reinterpret_cast<const std::uint8_t*>(after.data()), after.size()));
    expect(!small.next(), "and it stays broken");
}

void requestTests()
{
    Request query = parseRequest(encodeQuery(7, "report \"x\"", 20));
    auto q = std::get_if<QueryRequest>(&query);
    expect(q && q->id == 7 && q->text == "report \"x\"" && q->limit == 20, "a query round-trips");
    Request big = parseRequest(encodeQuery(1, "a", 100000));
    expect(std::holds_alternative<QueryRequest>(big) && std::get<QueryRequest>(big).limit == kMaxResults, "the limit is capped");
    Request plain = parseRequest(R"({"jsonrpc":"2.0","id":2,"method":"query","params":{"text":"a"}})");
    expect(std::holds_alternative<QueryRequest>(plain) && std::get<QueryRequest>(plain).limit == kDefaultResults, "a default limit");
    Request status = parseRequest(encodeStatusRequest(3));
    expect(std::holds_alternative<StatusRequest>(status) && std::get<StatusRequest>(status).id == 3, "a status request");
    Request update = parseRequest(encodeUpdateRequest(6));
    expect(std::holds_alternative<UpdateRequest>(update) && std::get<UpdateRequest>(update).id == 6, "an update request names nothing");

    auto bad = [](std::string_view body, int code, bool hasId) {
        Request r = parseRequest(body);
        const BadRequest* b = std::get_if<BadRequest>(&r);
        return b && b->code == code && b->id.has_value() == hasId;
    };
    expect(bad("nope", kParseError, false), "not JSON");
    expect(bad("[1]", kInvalidRequest, false), "not an object");
    expect(bad(R"({"jsonrpc":"2.0","method":"status"})", kInvalidRequest, false), "no id");
    expect(bad(R"({"jsonrpc":"2.0","id":1.5,"method":"status"})", kInvalidRequest, false), "a fractional id");
    expect(bad(R"({"jsonrpc":"1.0","id":1,"method":"status"})", kInvalidRequest, true), "the wrong version");
    expect(bad(R"({"jsonrpc":"2.0","id":1,"method":"delete"})", kMethodNotFound, true), "an unknown method");
    expect(bad(R"({"jsonrpc":"2.0","id":1,"method":"query"})", kInvalidParams, true), "a query without text");
    expect(bad(R"({"jsonrpc":"2.0","id":1,"method":"query","params":{"text":"a","limit":0}})", kInvalidParams, true), "a zero limit");
    expect(bad(R"({"jsonrpc":"2.0","id":1,"method":"query","params":[1]})", kInvalidParams, true), "params not an object");
    expect(bad(std::string(kMaxRequestBytes + 1, ' '), kInvalidRequest, false), "a request too long");
}

void answerTests()
{
    std::vector<ResultRow> rows{ { "report.pdf", "C:\\Docs\\report.pdf", false, false }, { "Docs", "C:\\Docs", true, true } };
    auto r = parseResponse(encodeResults(9, rows));
    expect(r && r->id == 9 && r->rows && *r->rows == rows && !r->volumes && !r->error, "results round-trip");
    std::vector<VolumeStatus> volumes{ { "C:\\", VolumeState::Ready, 123456 }, { "D:\\", VolumeState::Scanning, 0 } };
    auto s = parseResponse(encodeStatus(4, "0.8.0", volumes));
    expect(s && s->volumes && *s->volumes == volumes && s->protocol == kProtocolVersion && s->version == "0.8.0",
           "status round-trips, with the service's version");
    for (UpdateAnswer a : { UpdateAnswer::Started, UpdateAnswer::Current, UpdateAnswer::Busy, UpdateAnswer::DevBuild, UpdateAnswer::Failed }) {
        auto u = parseResponse(encodeUpdate(5, a));
        expect(u && u->id == 5 && u->update == a && !u->rows && !u->volumes, "an update answer round-trips");
    }
    expect(!parseResponse(R"({"jsonrpc":"2.0","id":1,"result":{"update":"maybe"}})"), "an unknown update answer is refused");
    auto e = parseResponse(encodeError(std::nullopt, kParseError, "not JSON"));
    expect(e && e->error == "not JSON", "an error, even without an id");
    expect(!parseResponse("{}") && !parseResponse("{\"id\":1,\"result\":{\"hits\":[{\"name\":1}]}}"), "malformed answers are refused");

    // An answer never passes the size cap: rows are dropped from the end.
    std::vector<ResultRow> huge(kMaxResults, ResultRow{ "n", std::string(60000, 'p'), false, false });
    std::string body = encodeResults(1, huge);
    auto cut = parseResponse(body);
    expect(body.size() <= kMaxResponseBytes && cut && cut->rows && !cut->rows->empty() && cut->rows->size() < huge.size(),
           "a huge answer is cut to fit");
}

void accessTests()
{
    int checks = 0;
    FolderAccess access([&](const std::string& path) {
        ++checks;
        return path != "C:\\Users\\other";
    });
    expect(access.visible(0, 10, "C:\\Docs", 0) && !access.visible(0, 11, "C:\\Users\\other", 0) && checks == 2, "asks once per folder");
    expect(access.visible(0, 10, "C:\\Docs", 1000) && !access.visible(0, 11, "C:\\Users\\other", 1000) && checks == 2, "then remembers");
    expect(access.visible(1, 10, "D:\\Docs", 1000) && checks == 3, "the same record on another drive is another folder");
    expect(access.visible(0, 10, "C:\\Docs", kFolderAccessMs) && checks == 4, "answers expire");
    expect(access.visible(0, 10, "C:\\Docs", 0) && checks == 5, "a clock going back starts over too");
    for (std::uint32_t f = 0; f < kMaxCachedFolders + 5; ++f) access.visible(2, f, "E:\\x", 1);
    expect(access.cached() <= kMaxCachedFolders, "the cache has a ceiling");
}

void mergeTests()
{
    std::vector<std::vector<Hit>> lists{
        { { 1, 4, false, 6 }, { 2, 2, false, 6 } },
        { { 1, 4, false, 6 }, { 9, 3, true, 4 }, { 8, 1, false, 3 } },
    };
    auto picks = mergeHits(lists, 10);
    std::vector<std::pair<std::size_t, std::size_t>> order;
    for (const Pick& p : picks) order.push_back({ p.list, p.index });
    expect(order == std::vector<std::pair<std::size_t, std::size_t>>{ { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { 1, 2 } },
           "by score across drives; equal hits keep the drives' order");
    expect(mergeHits(lists, 2).size() == 2 && mergeHits({}, 5).empty(), "the limit, and no drives");
}

} // namespace

void runIndexerProtocolTests()
{
    jsonTests();
    framingTests();
    requestTests();
    answerTests();
    accessTests();
    mergeTests();
}

} // namespace cup::test
