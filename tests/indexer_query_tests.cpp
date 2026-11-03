// The file index's queries and its file on disk (decision 075).

#include "expect.h"
#include "indexer/query.h"
#include "indexer/snapshot.h"

#include <algorithm>
#include <random>

namespace cup::test {

using namespace cup::indexer;

namespace {

// C:\Music (40), C:\Music\Q3-report.pdf (41), C:\Docs (42), C:\Docs\report (43,
// a folder), C:\Docs\report.docx (44), C:\Docs\reporter.txt (45), C:\notes.txt (46).
FileIndex sampleIndex()
{
    FileIndex index;
    FileRef root = refOf(kRootRecord, 5);
    index.put(refOf(40, 1), root, "Music", kFolder);
    index.put(refOf(41, 1), refOf(40, 1), "Q3-report.pdf", 0);
    index.put(refOf(42, 1), root, "Docs", kFolder);
    index.put(refOf(43, 1), refOf(42, 1), "report", kFolder);
    index.put(refOf(44, 1), refOf(42, 1), "Report.DOCX", 0);
    index.put(refOf(45, 1), refOf(42, 1), "reporter.txt", 0);
    index.put(refOf(46, 1), root, "notes.txt", 0);
    return index;
}

// The snapshot's closing checksum (FNV-1a), so damaged bodies can be sealed
// again and reach the parser instead of stopping at the checksum.
std::string resealed(std::string body)
{
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (char c : body) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 0x100000001b3ull;
    }
    for (int i = 0; i < 8; ++i) body += static_cast<char>((hash >> (8 * i)) & 0xFF);
    return body;
}

std::vector<std::uint32_t> records(const std::vector<Hit>& hits)
{
    std::vector<std::uint32_t> out;
    for (const Hit& h : hits) out.push_back(h.record);
    return out;
}

void parseTests()
{
    Query q = parseQuery("  Report \"my music\" ext:PDF;.docx,txt path:Docs folder: file:");
    expect(q.words == std::vector<std::string>{ "report", "my music" }, "words lower-cased, a quoted phrase is one");
    expect(q.extensions == std::vector<std::string>{ "pdf", "docx", "txt" }, "ext: takes ; or , and an optional dot");
    expect(q.pathWords == std::vector<std::string>{ "docs" } && q.kind == Kind::Files, "path:, and the last of file:/folder:");
    Query f = parseQuery("folder:music");
    expect(f.kind == Kind::Folders && f.words == std::vector<std::string>{ "music" }, "folder:x is a folder named like x");
    expect(parseQuery("http://x").words == std::vector<std::string>{ "http://x" }, "other colons are plain text");
    expect(parseQuery("path:\"my music\"").pathWords == std::vector<std::string>{ "my music" }, "a quoted filter value");
    expect(parseQuery("   ").empty() && parseQuery("file:").empty() && parseQuery("ext:").empty(), "nothing to look for");
    expect(parseQuery(std::string(10000, 'a')).words[0].size() == kMaxQueryLength, "long text is cut");
    std::string many;
    for (int i = 0; i < 40; ++i) many += "w" + std::to_string(i) + " ";
    expect(parseQuery(many).words.size() == kMaxTerms, "extra terms are dropped");
}

void matchTests()
{
    expect(matchScore("Report", "report") == 4, "the whole name, any ASCII case");
    expect(matchScore("reporter.txt", "report") == 3, "the start of the name");
    expect(matchScore("Q3-report.pdf", "report") == 2, "the start of a word");
    expect(matchScore("myreport.pdf", "report") == 1, "anywhere");
    expect(matchScore("myreport report", "report") == 2, "the best place counts, not the first");
    expect(matchScore("notes.txt", "report") == 0 && matchScore("rep", "report") == 0, "not in it");
    expect(matchScore("CAFÉ", "café") == 0 && matchScore("café", "café") == 4, "non-ASCII matches only as typed (v1)");
    expect(matchScore("notepad.exe", "notepad") == 4 && matchScore("Notepad.EXE", "notepad") == 4,
           "a file's name before its extension is a whole-name match");
    expect(matchScore("notepad.exe.mui", "notepad") == 3, "only the last extension comes off");
    expect(matchScore("v1.2", "v1", true) == 3 && matchScore("v1.2", "v1") == 4, "a folder's dot isn't an extension");
    expect(matchScore(".gitignore", "") == 0 && matchScore(".gitignore", "gitignore") == 2, "a leading dot isn't an extension");
}

void searchTests()
{
    FileIndex index = sampleIndex();
    auto find = [&](const char* text, std::size_t limit = 50) { return records(search(index, parseQuery(text), limit)); };

    expect(find("report") == std::vector<std::uint32_t>{ 43, 44, 45, 41 },
           "whole name (the folder first), then name starts (shorter first), then word starts");
    expect(find("report", 2) == std::vector<std::uint32_t>{ 43, 44 }, "the limit keeps the best");
    expect(find("report ext:pdf") == std::vector<std::uint32_t>{ 41 }, "an extension filter");
    expect(find("ext:txt") == std::vector<std::uint32_t>{ 46, 45 }, "a filter alone finds everything it fits");
    expect(find("report file:") == std::vector<std::uint32_t>{ 44, 45, 41 }, "file: leaves folders out");
    expect(find("folder:report") == std::vector<std::uint32_t>{ 43 }, "folder: keeps only folders");
    expect(find("report path:docs") == std::vector<std::uint32_t>{ 43, 44, 45 }, "path: looks at the folder");
    expect(find("q3 report") == std::vector<std::uint32_t>{ 41 }, "every word must match");
    expect(find("nothing-like-this").empty() && find("").empty(), "no match, or nothing typed");
    expect(search(index, parseQuery("report"), 0).empty(), "a zero limit");
}

void rankingTests()
{
    // C:\src\report.js (52), C:\app\node_modules\lib\report.js (55), and
    // C:\app\node_modules itself (53), next to C:\Windows\WinSxS\x\notepad.exe.
    FileIndex index;
    FileRef root = refOf(kRootRecord, 5);
    index.put(refOf(50, 1), root, "src-and-more", kFolder);
    index.put(refOf(51, 1), root, "app", kFolder);
    index.put(refOf(52, 1), refOf(50, 1), "report.js", 0);
    index.put(refOf(53, 1), refOf(51, 1), "node_modules", kFolder);
    index.put(refOf(54, 1), refOf(53, 1), "lib", kFolder);
    index.put(refOf(55, 1), refOf(54, 1), "report.js", 0);
    index.put(refOf(56, 1), refOf(55, 1), "x", 0); // a file's "child": corrupt, but harmless
    index.put(refOf(60, 1), root, "Windows", kFolder);
    index.put(refOf(61, 1), refOf(60, 1), "WinSxS", kFolder);
    index.put(refOf(62, 1), refOf(61, 1), "amd64_notepad", kFolder);
    index.put(refOf(63, 1), refOf(62, 1), "notepad.exe", 0);
    index.put(refOf(64, 1), refOf(60, 1), "notepad.exe", 0);
    index.put(refOf(65, 1), root, "Notepad-notes.txt", 0);

    expect(noiseFolderName("node_modules") && noiseFolderName("WinSxS") && noiseFolderName(".GIT") &&
           !noiseFolderName("node_modules2") && !noiseFolderName("src"), "noise folders are matched whole, any case");
    expect((index.entry(53).flags & kNoise) && !(index.entry(55).flags & kNoise), "only folders get the noise flag");
    expect(index.inNoise(55) && index.inNoise(54) && !index.inNoise(53) && !index.inNoise(52), "inside one, not the folder itself");

    auto find = [&](const char* text) { return records(search(index, parseQuery(text), 50)); };
    expect(find("report") == std::vector<std::uint32_t>{ 52, 55 }, "the same match inside node_modules comes last");
    expect(find("notepad") == std::vector<std::uint32_t>{ 64, 65, 63, 62 },
           "notepad.exe first (a whole-name match), WinSxS's copies after everything");
    expect(find("report path:node_modules") == std::vector<std::uint32_t>{ 55 }, "asked for by path: it's not demoted");
    expect(find("node_modules") == std::vector<std::uint32_t>{ 53 }, "the noise folder itself is found as usual");

    // More noise hits than the sorted head holds (limit 1: a head of 20):
    // the clean hit, though scored lower, still comes first.
    FileIndex crowded;
    crowded.put(refOf(10, 1), root, "node_modules", kFolder);
    for (std::uint32_t r = 11; r < 51; ++r) crowded.put(refOf(r, 1), refOf(10, 1), "report", 0);
    crowded.put(refOf(60, 1), root, "my-report.txt", 0);
    expect(records(search(crowded, parseQuery("report"), 1)) == std::vector<std::uint32_t>{ 60 },
           "a clean hit past a head full of noise");
    auto all = search(crowded, parseQuery("report"), 100);
    expect(all.size() == 41 && all[0].record == 60 && all[1].record == 11 && all[1].noise && all[40].record == 50,
           "then the noise, in order");

    // Ties go to the shorter name in characters, not bytes.
    FileIndex names;
    names.put(refOf(10, 1), root, "Змея.pdf", 0);      // 8 characters, 12 bytes
    names.put(refOf(11, 1), root, "abcdefghi.pdf", 0); // 13 characters
    names.put(refOf(12, 1), root, "abcdefghij.pdf", 0);
    expect(records(search(names, parseQuery("ext:pdf"), 10)) == std::vector<std::uint32_t>{ 10, 11, 12 }, "lengths count letters");

    // A renamed folder gains or loses the flag; a snapshot recomputes it.
    index.put(refOf(53, 1), refOf(51, 1), "packages", kFolder);
    expect(!index.inNoise(55), "renamed away from node_modules: not noise");
    index.put(refOf(53, 1), refOf(51, 1), "node_modules", kFolder);
    auto loaded = loadSnapshot(saveSnapshot(index, {}));
    expect(loaded && loaded->index.inNoise(55), "and back, through a snapshot");
}

void snapshotTests()
{
    FileIndex index = sampleIndex();
    index.remove(refOf(46, 1));
    index.put(refOf(45, 1), refOf(42, 1), "renamed.txt", kHidden);
    JournalMark mark{ 0x1122334455667788ull, 987654321 };
    std::string bytes = saveSnapshot(index, mark);
    auto loaded = loadSnapshot(bytes);
    expect(loaded.has_value(), "a snapshot loads back");
    if (!loaded) return;
    expect(loaded->mark == mark && loaded->index.count() == index.count(), "the mark and every entry");
    for (std::uint32_t r = 0; r < index.slots(); ++r) {
        if (!index.used(r)) continue;
        expect(loaded->index.path(r, "C:\\") == index.path(r, "C:\\") && loaded->index.entry(r).flags == index.entry(r).flags &&
               loaded->index.entry(r).sequence == index.entry(r).sequence, "each entry as it was");
    }
    expect(loadSnapshot(saveSnapshot(FileIndex{}, {}))->index.count() == 0, "an empty index");

    // Damage of any kind is refused whole (the caller then rescans).
    expect(!loadSnapshot("") && !loadSnapshot("CUPIDX"), "too short");
    expect(!loadSnapshot(bytes.substr(0, bytes.size() - 1)), "cut off");
    std::string flipped = bytes;
    flipped[30] ^= 0x01;
    expect(!loadSnapshot(flipped), "a changed byte");
    std::string other = bytes;
    other[0] = 'X';
    expect(!loadSnapshot(other), "not our file");

    std::string body = bytes.substr(0, bytes.size() - 8);
    expect(loadSnapshot(resealed(body)).has_value(), "resealing an intact body changes nothing");
    expect(!loadSnapshot(resealed(body + "x")), "trailing bytes");
    std::string reordered = body;
    reordered[28] = static_cast<char>(0x20); // the entry count: more entries than the file holds
    expect(!loadSnapshot(resealed(reordered)), "a count past the end");
    std::string tooMany = body;
    tooMany[27] = static_cast<char>(0x7F); // slots past kMaxRecords
    expect(!loadSnapshot(resealed(tooMany)), "a slot count past the cap");

    // Damaged entries with a valid checksum never crash the loader, and what
    // loads is a consistent index. (The header is left alone: a bigger slot
    // count is valid, just big.)
    std::mt19937 random(75);
    int loadedCount = 0;
    for (int i = 0; i < 2000; ++i) {
        std::string junk = body;
        for (int j = 0; j < 3; ++j) junk[32 + random() % (junk.size() - 32)] = static_cast<char>(random());
        if (random() % 4 == 0) junk.resize(32 + random() % (junk.size() - 31));
        if (auto s = loadSnapshot(resealed(junk))) {
            ++loadedCount;
            std::size_t used = 0;
            for (std::uint32_t r = 0; r < s->index.slots(); ++r) used += s->index.used(r);
            expect(used == s->index.count(), "a loaded index counts what it holds");
        }
    }
    expect(loadedCount < 2000, "most damage is refused");
}

} // namespace

void runIndexerQueryTests()
{
    parseTests();
    matchTests();
    searchTests();
    rankingTests();
    snapshotTests();
}

} // namespace cup::test
