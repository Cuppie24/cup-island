#pragma once
// LRCLIB (lrclib.net), the free, keyless lyrics service (decision 180): what
// to ask it and which of its answers to take. Pure: the request itself is
// lrclib_fetch's, on Windows.

#include "core/content.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

// The track whose lyrics are wanted, as the media session names it.
struct LyricsQuery {
    std::string title;
    std::string artist;
    Ms duration{};
    bool operator==(const LyricsQuery&) const = default;
};

// Whether it's worth asking: a title, an artist (without one it's a video
// or a stream, and a title alone matches anything) and a length (an app
// that reports no timeline gives no place in the song to follow either).
bool worthLooking(const LyricsQuery& query);

// https://lrclib.net/api/search?track_name=…&artist_name=…, percent-encoded.
// The search, not /api/get: that one wants the album and the exact length,
// which media sessions often don't have; the length is checked here instead.
std::string lrclibSearchUrl(const LyricsQuery& query);

// One of the search's records: only what's used.
struct LrclibRecord {
    Ms duration{};
    std::string synced; // its "syncedLyrics", LRC text; empty: plain lyrics only, or instrumental
    bool operator==(const LrclibRecord&) const = default;
};

// The timed lyrics of the record nearest the track's length, within
// lyricsDurationTolerance: another cut of the song (a live take, a radio
// edit) would run out of step. Nothing: no record with timed lyrics fits.
std::optional<std::string> pickSynced(const std::vector<LrclibRecord>& records, Ms duration);

} // namespace cup
