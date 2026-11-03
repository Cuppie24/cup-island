// The file index (decision 075): USN records parsed from raw bytes, the
// record-addressed index, journal changes and resume-or-rescan.

#include "expect.h"
#include "indexer_fixtures.h"
#include "indexer/journal.h"

namespace cup::test {

using namespace cup::indexer;

namespace {

void recordTests()
{
    // Two V2 records after the 8-byte "next", as ENUM_USN_DATA returns them.
    std::vector<std::uint8_t> buffer;
    appendU64(buffer, 0x77);
    appendRecordV2(buffer, refOf(40, 3), refOf(kRootRecord, 5), 1000, 0, 0x10, u"Music");
    appendRecordV2(buffer, refOf(41, 1), refOf(40, 3), 1001, kReasonFileCreate, 0x20, u"song.mp3");
    UsnBuffer b = parseUsnBuffer(buffer);
    expect(b.ok && b.next == 0x77 && b.records.size() == 2, "a buffer's next mark and its records");
    expect(b.records[0].file == refOf(40, 3) && b.records[0].parent == refOf(kRootRecord, 5), "V2 references");
    expect(b.records[0].name == "Music" && b.records[0].attributes == 0x10 && b.records[0].usn == 1000, "V2 fields");
    expect(b.records[1].reason == kReasonFileCreate && b.records[1].name == "song.mp3", "the second record's reason and name");

    // V3: 128-bit references (ReFS-sized), same fields otherwise.
    std::vector<std::uint8_t> v3;
    appendU64(v3, 9);
    appendRecordV3(v3, { 12, 34 }, { 56, 78 }, 5, kReasonRenameNewName, 0, u"new.txt");
    b = parseUsnBuffer(v3);
    expect(b.ok && b.records.size() == 1 && b.records[0].file == FileRef{ 12, 34 } &&
           b.records[0].parent == FileRef{ 56, 78 } && b.records[0].name == "new.txt" &&
           b.records[0].reason == kReasonRenameNewName, "V3 records");

    // Names: UTF-16 to UTF-8, surrogate pairs joined, a lone one replaced.
    std::vector<std::uint8_t> named;
    appendU64(named, 0);
    appendRecordV2(named, refOf(50, 1), refOf(kRootRecord, 5), 0, 0, 0, u"café 🎵");
    expect(parseUsnBuffer(named).records[0].name == "caf\xC3\xA9 \xF0\x9F\x8E\xB5", "non-ASCII names in UTF-8");
    const std::uint8_t lone[] = { 0x3D, 0xD8, 'a', 0 };
    expect(utf16leToUtf8(lone) == "\xEF\xBF\xBD" "a", "a lone surrogate becomes U+FFFD");

    // Bad buffers: whatever came before the bad record is kept.
    expect(!parseUsnBuffer(std::vector<std::uint8_t>(4)).ok, "shorter than the next mark");
    std::vector<std::uint8_t> zero = buffer;
    appendU64(zero, 0); // a record of length 0 would loop for ever
    b = parseUsnBuffer(zero);
    expect(!b.ok && b.records.size() == 2, "a zero-length record stops the parse");
    std::vector<std::uint8_t> cut(buffer.begin(), buffer.end() - 8);
    b = parseUsnBuffer(cut);
    expect(!b.ok && b.records.size() == 1, "a record cut short is dropped");
    std::vector<std::uint8_t> badName = named;
    badName[8 + 56] = 0xFF; // FileNameLength past the record's end
    expect(!parseUsnBuffer(badName).ok, "a name past its record is refused");
    std::vector<std::uint8_t> v4;
    appendU64(v4, 0);
    appendU32(v4, 16);
    appendU16(v4, 4);
    appendU16(v4, 0);
    appendU64(v4, 0);
    appendRecordV2(v4, refOf(60, 1), refOf(kRootRecord, 5), 0, 0, 0, u"after.txt");
    b = parseUsnBuffer(v4);
    expect(b.ok && b.records.size() == 1 && b.records[0].name == "after.txt", "V4 range records are skipped");
}

void indexTests()
{
    FileIndex index;
    expect(index.put(refOf(40, 3), refOf(kRootRecord, 5), "Music", kFolder), "a folder goes in");
    expect(index.put(refOf(41, 1), refOf(40, 3), "song.mp3", 0), "a file in it");
    expect(index.count() == 2 && index.slots() == 42, "addressed by record number");
    expect(index.path(41, "C:\\") == "C:\\Music\\song.mp3", "paths walk up to the root");
    expect(index.path(40, "D:\\") == "D:\\Music", "a folder in the root");
    expect(!index.put(refOf(kRootRecord, 5), refOf(kRootRecord, 5), ".", kFolder), "the root itself is the volume, not an entry");
    expect(!index.put(FileRef{ 1, 1 }, refOf(kRootRecord, 5), "refs.txt", 0), "128-bit references are refused for now");
    expect(!index.put(refOf(kMaxRecords, 1), refOf(kRootRecord, 5), "far", 0) && index.slots() == 42,
           "a record past the cap asks for no memory");
    expect(!index.put(refOf(42, 1), refOf(kRootRecord, 5), "", 0), "no empty names");

    // Child before parent (enumeration order is by record number).
    index.put(refOf(70, 1), refOf(80, 1), "early.txt", 0);
    expect(index.path(70, "C:\\") == "C:\\early.txt", "a missing parent ends the path early");
    index.put(refOf(80, 1), refOf(40, 3), "Late", kFolder);
    expect(index.path(70, "C:\\") == "C:\\Music\\Late\\early.txt", "and the full path once it arrives");

    // Sequence numbers: a record reused for another file.
    expect(index.find(refOf(41, 1)) == 41u && !index.find(refOf(41, 2)), "found only with its own sequence");
    expect(!index.remove(refOf(41, 2)) && index.used(41), "a delete of another use of the record does nothing");
    expect(index.remove(refOf(41, 1)) && !index.used(41) && index.count() == 3, "a delete of this one removes it");
    expect(index.name(41).empty() && index.path(41, "C:\\").empty(), "nothing left at a removed record");

    // Renames append; unchanged names don't; compaction gets the space back.
    FileIndex tiny;
    tiny.put(refOf(10, 1), refOf(kRootRecord, 1), "aaaaaaaaaa", 0);
    tiny.put(refOf(11, 1), refOf(kRootRecord, 1), "bbbbbbbbbb", 0);
    std::size_t before = tiny.memoryBytes();
    for (int i = 0; i < 100; ++i) tiny.put(refOf(10, 1), refOf(kRootRecord, 1), "aaaaaaaaaa", kHidden);
    expect(tiny.memoryBytes() == before && (tiny.entry(10).flags & kHidden), "the same name again only updates flags");
    for (int i = 0; i < 1000; ++i) tiny.put(refOf(10, 1), refOf(kRootRecord, 1), i % 2 ? "cccccccccc" : "dddddddddd", 0);
    expect(tiny.name(10) == "cccccccccc" && tiny.name(11) == "bbbbbbbbbb", "names stay right through compactions");
    expect(tiny.memoryBytes() < before + 200, "replaced names don't pile up");

    // A loop in corrupt data ends.
    FileIndex looped;
    looped.put(refOf(20, 1), refOf(21, 1), "a", kFolder);
    looped.put(refOf(21, 1), refOf(20, 1), "b", kFolder);
    expect(!looped.path(20, "C:\\").empty(), "a parent loop doesn't hang");

    expect(flagsFromAttributes(0x10 | 0x2 | 0x4 | 0x20) == (kFolder | kHidden | kSystem), "attributes to flags");
}

void journalTests()
{
    FileIndex index;
    auto rec = [](FileRef file, FileRef parent, std::uint32_t reason, const char* name) {
        return UsnRecord{ file, parent, 0, reason, 0, name };
    };
    FileRef root = refOf(kRootRecord, 5);
    expect(applyRecord(index, rec(refOf(30, 1), root, 0, "a.txt")) == Change::Added, "an enumerated file is added");
    expect(applyRecord(index, rec(refOf(31, 1), root, kReasonFileCreate, "b.txt")) == Change::Added, "a created file is added");
    expect(applyRecord(index, rec(refOf(30, 1), root, kReasonRenameOldName, "a.txt")) == Change::Ignored &&
           index.name(30) == "a.txt", "a rename's old name changes nothing");
    expect(applyRecord(index, rec(refOf(30, 1), root, kReasonRenameNewName, "z.txt")) == Change::Updated &&
           index.name(30) == "z.txt", "its new name renames");
    expect(applyRecord(index, rec(refOf(31, 1), root, kReasonFileDelete, "b.txt")) == Change::Removed && !index.used(31),
           "a delete removes");
    expect(applyRecord(index, rec(refOf(32, 1), root, kReasonFileCreate | kReasonFileDelete, "tmp")) == Change::Ignored &&
           !index.used(32), "made and deleted between reads: never shown");
    expect(applyRecord(index, rec(refOf(31, 1), root, kReasonFileDelete, "b.txt")) == Change::Ignored, "a second delete is a no-op");

    JournalMark mark{ 7, 500 };
    expect(resumeFrom(mark, { 7, 100, 900 }) == Resume::Replay, "the same journal, still holding the mark: replay");
    expect(resumeFrom(mark, { 7, 500, 500 }) == Resume::Replay, "nothing new since: replay");
    expect(resumeFrom(mark, { 8, 100, 900 }) == Resume::Rescan, "a re-created journal: rescan");
    expect(resumeFrom(mark, { 7, 600, 900 }) == Resume::Rescan, "wrapped past the mark: rescan");
    expect(resumeFrom(mark, { 7, 100, 400 }) == Resume::Rescan, "behind the mark (a restored image): rescan");
}

} // namespace

void runIndexerTests()
{
    recordTests();
    indexTests();
    journalTests();
}

} // namespace cup::test
