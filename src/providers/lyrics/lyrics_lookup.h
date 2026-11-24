#pragma once
// Which songs' lyrics are known, being looked up, or not to be had (decision
// 180): one lookup per song, its answer remembered for the songs played
// lately. Pure: the request is handed in (lrclib_fetch on Windows, a fake in
// tests).

#include "providers/lyrics/lrclib.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cup {

using LyricLines = std::shared_ptr<const std::vector<LyricLine>>;

struct LyricsAnswer {
    enum class State { None, Looking, Found };
    State state = State::None;
    LyricLines lines; // Found only
};

class LyricsLookup {
public:
    // Asks the service for a song's records. `done` comes later, on the UI
    // thread: the records (maybe none), or nothing if the request failed.
    using Done = std::function<void(std::optional<std::vector<LrclibRecord>>)>;
    using Fetch = std::function<void(const LyricsQuery&, Done)>;

    // `changed`: a lookup ended, so lyricsFor() may answer differently now.
    LyricsLookup(Fetch fetch, std::function<void()> changed);
    ~LyricsLookup();

    // What's known of this song's lyrics; the first time, starts looking.
    // A lookup that failed (no network) is tried again the next time the
    // song comes up after another one, not on every call.
    LyricsAnswer lyricsFor(const LyricsQuery& query);

private:
    struct Entry {
        std::string key;
        LyricsAnswer answer;
        bool failed = false;
    };
    Entry* find(const std::string& key);
    void landed(const std::string& key, const LyricsQuery& query, std::optional<std::vector<LrclibRecord>> records);

    Fetch fetch_;
    std::function<void()> changed_;
    std::vector<Entry> remembered_; // the latest asked last; lyricsRemembered at most
    std::string current_;           // the song asked about last
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); // false once gone: late answers are dropped
};

} // namespace cup
