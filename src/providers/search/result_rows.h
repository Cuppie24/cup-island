#pragma once
// How a search result reads on the card (decision 079): two lines (its name,
// then where it is), the parts the query matched brighter, the folder you're
// in shortened to ~. Pure, tested on Linux.

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace cup {

// Where in `name` the typed words are, as (byte offset, byte length), in
// order and not overlapping: each word's first match at the start of a word
// in the name ("rep" in "q3-report"), else its first match anywhere. A path
// word ("projects/cwatis") counts by its last part, as search ranks it.
// Case-folded as search matches (English and Russian).
std::vector<std::pair<std::size_t, std::size_t>> matchedParts(const std::string& name, const std::string& query);

// `path` with the user's own folder (`home`, "C:\Users\Said") as "~":
// "~\Documents\report.pdf". Other paths as they are.
std::string shortPath(const std::string& path, const std::string& home);

// The folder a path is in ("C:\a\b.txt": "C:\a"; "C:\a": "C:\").
std::string folderOf(const std::string& path);

} // namespace cup
