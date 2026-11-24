#include "providers/lyrics/lyrics_lookup.h"

#include "core/tokens.h"
#include "providers/lyrics/lrc.h"

#include <algorithm>

namespace cup {
namespace {

// A song by its names: its length may arrive after them, or differ by a moment between reads.
std::string keyOf(const LyricsQuery& query) { return query.artist + "\n" + query.title; }

} // namespace

LyricsLookup::LyricsLookup(Fetch fetch, std::function<void()> changed) : fetch_(std::move(fetch)), changed_(std::move(changed)) {}

LyricsLookup::~LyricsLookup() { *alive_ = false; }

LyricsLookup::Entry* LyricsLookup::find(const std::string& key)
{
    auto it = std::find_if(remembered_.begin(), remembered_.end(), [&](const Entry& e) { return e.key == key; });
    return it == remembered_.end() ? nullptr : &*it;
}

LyricsAnswer LyricsLookup::lyricsFor(const LyricsQuery& query)
{
    if (!worthLooking(query) || !fetch_) return {};
    std::string key = keyOf(query);
    bool cameUp = key != current_; // not the song asked about last time
    current_ = key;
    if (Entry* known = find(key)) {
        if (!(known->failed && cameUp)) return known->answer;
        remembered_.erase(remembered_.begin() + (known - remembered_.data())); // failed last time: once more
    }
    remembered_.push_back({ key, { LyricsAnswer::State::Looking, nullptr } });
    std::size_t most = std::max<std::size_t>(tokens().lyricsRemembered, 1);
    if (remembered_.size() > most) remembered_.erase(remembered_.begin(), remembered_.end() - static_cast<std::ptrdiff_t>(most));
    fetch_(query, [this, alive = alive_, key, query](std::optional<std::vector<LrclibRecord>> records) {
        if (*alive) landed(key, query, std::move(records));
    });
    // Answered at once (a fake): as it is now.
    Entry* now = find(key);
    return now ? now->answer : LyricsAnswer{};
}

void LyricsLookup::landed(const std::string& key, const LyricsQuery& query, std::optional<std::vector<LrclibRecord>> records)
{
    Entry* entry = find(key);
    if (!entry) return; // forgotten meanwhile: many songs skipped through
    entry->answer = {};
    entry->failed = !records;
    if (records)
        if (auto lrc = pickSynced(*records, query.duration)) {
            auto lines = std::make_shared<std::vector<LyricLine>>(parseLrc(*lrc));
            if (!lines->empty()) entry->answer = { LyricsAnswer::State::Found, std::move(lines) };
        }
    if (changed_) changed_();
}

} // namespace cup
