#pragma once
// What the user typed, and the names that match it, best first
// (decision 075). Case-insensitive for ASCII only: other letters must match
// as typed, until Unicode case folding is worth its tables.

#include "indexer/file_index.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cup::indexer {

// Longer text is cut here; more terms than kMaxTerms are dropped. A query is
// typed by hand, and the service must bound what one request can cost.
constexpr std::size_t kMaxQueryLength = 512;
constexpr std::size_t kMaxTerms = 16;

enum class Kind { Any, Files, Folders };

struct Query {
    std::vector<std::string> words;      // each must be in the name; lower case
    std::vector<std::string> extensions; // ext:pdf;docx: one must match, lower case, no dot
    std::vector<std::string> pathWords;  // path:music: each must be in the folder's path
    Kind kind = Kind::Any;               // file: / folder:
    bool empty() const { return words.empty() && extensions.empty() && pathWords.empty(); }
};

// Words are split on spaces; "quoted text" is one word. `ext:a;b` (or a,b),
// `path:x`, `file:` and `folder:` filter; `folder:x` is also the word x.
// Any other `a:b` is just a word.
Query parseQuery(std::string_view text);

struct Hit {
    std::uint32_t record = 0;
    int score = 0;
    bool folder = false;          // for ordering, and merging across drives
    std::uint16_t nameLength = 0; // likewise
};

// The order search() returns: higher score, then folders, then the shorter
// name, then the lower record.
bool betterHit(const Hit& a, const Hit& b);

// The best `limit` matches, best first. Score: each word adds 4 for the
// whole name, 3 for the start of the name, 2 for the start of a word in it
// ("report" in "q3-report.pdf"), 1 anywhere. Ties: folders first, then the
// shorter name, then the record (so the order never wobbles). An empty
// query finds nothing.
std::vector<Hit> search(const FileIndex& index, const Query& query, std::size_t limit);

// One hit picked from several lists (one per drive): which list, which hit.
struct Pick {
    std::size_t list = 0;
    std::size_t index = 0;
};
// The best `limit` hits across lists that each come from search(), in
// betterHit order; between equals, the earlier list (drive) first.
std::vector<Pick> mergeHits(const std::vector<std::vector<Hit>>& lists, std::size_t limit);

// The match class of one word in one name (0: not in it); what search() sums.
int matchScore(std::string_view name, std::string_view lowerWord);

} // namespace cup::indexer
