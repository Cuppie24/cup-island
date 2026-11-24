#include "providers/lyrics/lrclib_fetch.h"

#include "platform/log.h"
#include "platform/watchdog.h"
#include "platform/win.h"

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

#include <cmath>
#include <utility>

namespace cup {
namespace {

namespace http = winrt::Windows::Web::Http;
namespace json = winrt::Windows::Data::Json;
namespace crypto = winrt::Windows::Security::Cryptography;
using platform::logLine;

// LRCLIB asks every client to say what it is. The app's name and version,
// as the updater sends GitHub; nothing about the user.
http::HttpClient client(const std::string& appVersion)
{
    http::HttpClient c;
    c.DefaultRequestHeaders().UserAgent().TryParseAdd(L"CupIsland/" + winrt::to_hstring(appVersion));
    return c;
}

// LRCLIB's search answer: an array of records. Only those with timed lyrics
// are kept ("syncedLyrics" is null for plain or instrumental ones).
std::optional<std::vector<LrclibRecord>> parseRecords(const winrt::hstring& text)
{
    json::JsonArray array{ nullptr };
    if (!json::JsonArray::TryParse(text, array)) return std::nullopt;
    std::vector<LrclibRecord> records;
    for (uint32_t i = 0; i < array.Size(); ++i) {
        if (array.GetAt(i).ValueType() != json::JsonValueType::Object) continue;
        auto o = array.GetObjectAt(i); // not as<JsonObject>(), not GetObject(): see github.cpp
        if (!o.HasKey(L"syncedLyrics") || !o.HasKey(L"duration")) continue;
        auto synced = o.GetNamedValue(L"syncedLyrics"), duration = o.GetNamedValue(L"duration");
        if (synced.ValueType() != json::JsonValueType::String || duration.ValueType() != json::JsonValueType::Number) continue;
        records.push_back({ Ms{ std::llround(duration.GetNumber() * 1000.0) }, winrt::to_string(synced.GetString()) }); // seconds
    }
    return records;
}

winrt::fire_and_forget fetch(std::string appVersion, LyricsQuery query, LyricsLookup::Done done)
{
    winrt::apartment_context ui; // `done` runs back on this (the UI) thread
    std::optional<std::vector<LrclibRecord>> records;
    try {
        winrt::Windows::Foundation::Uri uri(winrt::to_hstring(lrclibSearchUrl(query)));
        auto response = co_await client(appVersion).GetAsync(uri);
        if (response.IsSuccessStatusCode()) {
            // The bytes as UTF-8, whatever the answer's headers say: read as a
            // string, a missing charset would turn every non-English line to noise.
            auto bytes = co_await response.Content().ReadAsBufferAsync();
            records = parseRecords(crypto::CryptographicBuffer::ConvertBinaryToString(crypto::BinaryStringEncoding::Utf8, bytes));
            if (records) logLine("lyrics: LRCLIB has %zu timed record(s)", records->size());
            else logLine("lyrics: LRCLIB's answer didn't parse");
        } else {
            logLine("lyrics: LRCLIB answered %d", static_cast<int>(response.StatusCode()));
        }
    } catch (const winrt::hresult_error& e) {
        logLine("lyrics: lookup failed: 0x%08x", static_cast<unsigned>(e.code())); // offline, most often
    }
    try {
        co_await ui;
    } catch (...) {
        co_return; // the app is closing: nobody to tell
    }
    // Out of a fire_and_forget an exception is a fail-fast (decision 158): logged instead.
    platform::guardedTask("after a lyrics lookup", [&] { done(std::move(records)); });
}

} // namespace

void fetchLrclib(const std::string& appVersion, const LyricsQuery& query, LyricsLookup::Done done)
{
    fetch(appVersion, query, std::move(done));
}

} // namespace cup
