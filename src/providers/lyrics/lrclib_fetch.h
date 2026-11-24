#pragma once
// LRCLIB's Windows side (decision 180): the search request itself. Windows
// only, compiled into the app; what to ask and which answer to take is
// lrclib.h's, when to ask LyricsLookup's.

#include "providers/lyrics/lyrics_lookup.h"

#include <string>

namespace cup {

// GET lrclibSearchUrl(query). Call on the UI thread; `done` runs there too:
// the records LRCLIB sent (maybe none), or nothing if it couldn't be asked.
void fetchLrclib(const std::string& appVersion, const LyricsQuery& query, LyricsLookup::Done done);

} // namespace cup
