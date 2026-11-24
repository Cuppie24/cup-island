#include "providers/lyrics/lrclib.h"

#include "core/tokens.h"

#include <cstdlib>

namespace cup {
namespace {

// Everything but letters, digits and "-_.~" as %XX of its UTF-8 bytes (RFC 3986).
std::string encoded(const std::string& text)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : text) {
        bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (plain) out += static_cast<char>(c);
        else out += { '%', kHex[c >> 4], kHex[c & 15] };
    }
    return out;
}

} // namespace

bool worthLooking(const LyricsQuery& query)
{
    return !query.title.empty() && !query.artist.empty() && query.duration > Ms{ 0 };
}

std::string lrclibSearchUrl(const LyricsQuery& query)
{
    return "https://lrclib.net/api/search?track_name=" + encoded(query.title) + "&artist_name=" + encoded(query.artist);
}

std::optional<std::string> pickSynced(const std::vector<LrclibRecord>& records, Ms duration)
{
    const LrclibRecord* best = nullptr;
    long long bestOff = 0;
    for (const LrclibRecord& record : records) {
        if (record.synced.empty()) continue;
        long long off = std::llabs((record.duration - duration).count());
        if (off > tokens().lyricsDurationTolerance.count()) continue;
        if (!best || off < bestOff) best = &record, bestOff = off; // the first of equals: LRCLIB's best match
    }
    return best ? std::optional<std::string>(best->synced) : std::nullopt;
}

} // namespace cup
