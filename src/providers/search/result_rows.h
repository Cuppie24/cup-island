#pragma once
// How a search result reads on the card (decision 079): two lines (its name,
// then where it is), the parts the query matched brighter, the folder you're
// in shortened to ~. Pure, tested on Linux.

#include <cstddef>
#include <functional>
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

// What the user typed with its shortcuts as paths the index understands:
// "~" is the user's own folder (`home`), the other way round from shortPath,
// and "%NAME%" an environment variable's value (`variable`: "" if unset):
// "~\\Documents", "%APPDATA%\\Code", "path:%TEMP%". The index service runs as
// SYSTEM and can't know whose they are, so the provider expands them first.
// Only one that starts a word and is followed by a separator, a quote, a
// space or nothing: "~notes.txt" and "100%" are words. A value with a space
// ("C:\\Program Files") is quoted so it stays one word. Unknown: as typed.
std::string expandShortcuts(const std::string& query, const std::string& home,
                            const std::function<std::string(const std::string& name)>& variable);

// The folder a path is in ("C:\a\b.txt": "C:\a"; "C:\a": "C:\").
std::string folderOf(const std::string& path);

} // namespace cup
