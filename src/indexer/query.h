#pragma once
// What the user typed, and the names that match it, best first
// (decision 075). Case-insensitive for English and Russian (А-Я; not Ё, and
// no other alphabets): both fold within the same byte length, so matching
// stays byte by byte and fast. Other letters match only as typed.

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

// A word with a '/' or '\\' in it ("projects/cwatis"): its parts are
// folder names in order, gaps allowed, each matching the start of a name.
// An item matches if its name starts with the last part and its folders
// hold the others ("projects/cwatis" finds the folder cwatis in
// D:\\Projects\\claude-micros-wiki), ranked like a name match; or if it's inside a folder that matches
// (its contents), ranked after, shorter paths first. Empty parts are dropped,
// so "documents/" is the Documents folders and what's in them. A drive letter
// only keeps the search to that drive ("d:/cwatis": cwatis anywhere on D:);
// a drive alone ("d:/") lists the drive's top level.
struct PathWord {
    std::vector<std::string> parts; // lower case, at least one
    bool operator==(const PathWord&) const = default;
};

struct Query {
    std::vector<std::string> words;      // each must be in the name; lower case
    std::vector<PathWord> paths;         // projects/cwatis: see PathWord
    std::vector<std::string> extensions; // ext:pdf;docx: one must match, lower case, no dot
    std::vector<std::string> pathWords;  // path:music: each must be in the folder's path
    Kind kind = Kind::Any;               // file: / folder:
    char drive = 0;                      // 'a'..'z' from a path word's drive ("d:/x"): only that drive
    bool empty() const { return words.empty() && paths.empty() && extensions.empty() && pathWords.empty() && !drive; }
    // Nothing to match but a drive ("d:/"): its top level.
    bool driveOnly() const { return drive && words.empty() && paths.empty() && extensions.empty() && pathWords.empty(); }
};

// Words are split on spaces; "quoted text" is one word. `ext:a;b` (or a,b),
// `path:x`, `file:` and `folder:` filter; `folder:x` is also the word x. A
// word with a separator is a path word (PathWord). Any other `a:b` is just a
// word.
Query parseQuery(std::string_view text);

struct Hit {
    std::uint32_t record = 0;
    int score = 0;
    bool folder = false;          // for ordering, and merging across drives
    std::uint16_t nameLength = 0; // in characters, likewise
    bool noise = false;           // inside a noise folder (file_index.h), likewise
    std::uint16_t depth = 0;      // folders above it (C:\x.txt: 1), likewise
};

// The order search() returns: outside noise folders first, then the higher
// score, then the shorter path (D:\Documents before D:\a\b\Documents), then
// folders, then the shorter name, then the lower record.
bool betterHit(const Hit& a, const Hit& b);

// The best `limit` matches, best first. Score: each word adds 4 for the
// whole name or, for a file, the name before its extension ("notepad" for
// notepad.exe), 3 for the start of the name, 2 for the start of a word in it
// ("report" in "q3-report.pdf"), 1 anywhere; a path word scores by its last
// part. Anything inside a noise folder (WinSxS, node_modules...) comes after
// everything else, unless a path: filter names one: then it's what was asked
// for. Ties: the shorter path, then folders, then the shorter name in
// characters (a Cyrillic letter is two bytes, but one letter), then the
// record (so the order never wobbles). An empty query finds nothing.
std::vector<Hit> search(const FileIndex& index, const Query& query, std::size_t limit);

// One hit picked from several lists (one per drive): which list, which hit.
struct Pick {
    std::size_t list = 0;
    std::size_t index = 0;
};
// The best `limit` hits across lists that each come from search(), in
// betterHit order; between equals, the earlier list (drive) first.
std::vector<Pick> mergeHits(const std::vector<std::vector<Hit>>& lists, std::size_t limit);

// Case-folded comparisons as matching makes them (English and Russian): does
// `text` start with / contain `lowerWord`?
bool startsFolded(std::string_view text, std::string_view lowerWord);
bool containsFolded(std::string_view text, std::string_view lowerWord);

// `text` with English and Russian capitals folded to small letters, as
// matching sees it: what a word must be in before matchScore (the launcher
// folds its initials with it, decision 077).
std::string foldCase(std::string_view text);

// The match class of one word in one name (0: not in it); what search()
// sums. A folder's name has no extension: "v1.2" is all name.
int matchScore(std::string_view name, std::string_view lowerWord, bool folder = false);

} // namespace cup::indexer
