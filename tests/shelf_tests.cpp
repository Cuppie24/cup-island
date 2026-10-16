// The Shelf (decision 061): what each zone would do with a drag, safe file
// names, the saved list, and the provider's drops, copies and actions
// through a recording host and hooks the test answers.

#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/shelf/drop_plan.h"
#include "providers/shelf/safe_names.h"
#include "providers/shelf/shelf_content.h"
#include "providers/shelf/shelf_provider.h"

namespace cup::test {
namespace {

using namespace shelf;
constexpr std::uint64_t kMB = 1024 * 1024;
const Limits kLimits{ .copyMaxBytes = 1024 * kMB, .storeMaxBytes = 10240 * kMB, .storeUsed = 0, .maxChecked = 200 };

DragItem file(const std::string& name, std::uint64_t mb) { return { DragKind::File, name, mb * kMB }; }
DragItem folder(const std::string& name) { return { DragKind::Folder, name, std::nullopt }; }
DragItem text(std::size_t bytes) { return { DragKind::Text, "note", bytes }; }

DragSummary drag(std::vector<DragItem> items, bool known = true)
{
    DragSummary d{ .items = items, .total = items.size(), .sizesKnown = known };
    return d;
}

void planTests()
{
    DropPlan p = planDrop(drag({ file("a.pdf", 2) }), kLimits);
    expect(p.link.enabled && p.link.label == "Link" && p.copy.enabled && p.copy.label == "Copy", "plan: one small file, both zones");
    expect(p.link.items == std::vector{ Handling::Link } && p.copy.items == std::vector{ Handling::Copy }, "plan: ...each does its thing");

    p = planDrop(drag({ file("film.mkv", 4300) }), kLimits);
    expect(p.link.enabled && !p.copy.enabled, "plan: over the copy limit, Copy is greyed");
    expect(p.copy.reason == "Too large to copy (4.2 GB, limit 1 GB)", "plan: ...and says why: " + p.copy.reason);

    p = planDrop(drag({ folder("Photos") }), kLimits);
    expect(p.link.enabled && !p.copy.enabled && p.copy.reason == "Folders can only be linked", "plan: a folder is link only");

    p = planDrop(drag({ text(30), { DragKind::Url, "example.com", 20 } }), kLimits);
    expect(!p.link.enabled && p.copy.enabled && p.copy.label == "Copy 2", "plan: text and links are copied (saved as files)");
    expect(p.link.reason.starts_with("Nothing to link"), "plan: ...Link says why it's greyed");

    p = planDrop(drag({ file("a", 1), file("b", 1), file("film", 4300) }), kLimits);
    expect(p.copy.enabled && p.copy.label == "Copy 2 · 1 too large, will be linked", "plan: mixed: " + p.copy.label);
    expect(p.copy.items == std::vector{ Handling::Copy, Handling::Copy, Handling::Link }, "plan: ...the big one linked");
    expect(p.copy.reason == "4.2 GB, limit 1 GB", "plan: ...its size on the line under: " + p.copy.reason);
    expect(p.link.label == "Link 3", "plan: ...Link takes them all");

    p = planDrop(drag({ file("a", 1), text(10) }), kLimits);
    expect(p.link.enabled && p.link.label == "Link 1 · 1 has no file, will be copied", "plan: Link copies what it can't link: " + p.link.label);
    expect(p.link.items == std::vector{ Handling::Link, Handling::Copy }, "plan: ...in its plan");

    p = planDrop(drag({ file("a", 1), text(2000 * kMB) }), kLimits);
    expect(p.copy.label == "Copy 1 · 1 skipped (too large)", "plan: neither zone takes it: skipped: " + p.copy.label);

    Limits nearlyFull = kLimits;
    nearlyFull.storeUsed = nearlyFull.storeMaxBytes - 3 * kMB;
    p = planDrop(drag({ file("a", 2), file("b", 2), file("c", 1) }), nearlyFull);
    expect(p.copy.items == std::vector{ Handling::Copy, Handling::Link, Handling::Copy }, "plan: the store fills in drop order");
    expect(p.copy.label == "Copy 2 · 1 over the shelf's storage, will be linked", "plan: ...and says so: " + p.copy.label);
    nearlyFull.storeUsed = nearlyFull.storeMaxBytes;
    expect(planDrop(drag({ file("a", 2) }), nearlyFull).copy.reason == "Shelf storage full", "plan: a full store greys Copy");

    DragSummary noRoom = drag({ file("a", 5) });
    noRoom.freeBytes = 1 * kMB;
    expect(planDrop(noRoom, kLimits).copy.reason == "Not enough free space", "plan: no room on the disk greys Copy");

    DragSummary checking = drag({ { DragKind::File, "a", std::nullopt } }, false);
    p = planDrop(checking, kLimits);
    expect(p.copy.enabled && p.copy.reason == "Checking sizes…", "plan: sizes not read yet: Copy waits for them, enabled");

    DragSummary huge = drag({ file("a", 1) });
    huge.total = 500;
    p = planDrop(huge, kLimits);
    expect(!p.copy.enabled && p.copy.reason == "Large drop: link only" && p.link.label == "Link 500", "plan: past the cap, link only");

    expect(formatBytes(512) == "512 bytes" && formatBytes(12 * 1024) == "12 KB" && formatBytes(350 * kMB) == "350 MB" &&
           formatBytes(1024 * kMB) == "1 GB", "plan: sizes read naturally");
}

void nameTests()
{
    expect(safeFileName("a/b:c*?.txt") == "a_b_c__.txt", "names: forbidden characters become _");
    expect(safeFileName("report. . ") == "report", "names: no trailing dots or spaces");
    expect(safeFileName("con.txt") == "_con.txt" && safeFileName("COM1") == "_COM1" && safeFileName("console") == "console",
           "names: reserved names are avoided, others kept");
    expect(safeFileName("   ") == "Untitled", "names: blank: Untitled");
    expect(safeFileName("ééééé", 3) == "ééé", "names: cut by characters, never mid-character");
    expect(textFileName("\n  Buy milk and bread\nmore") == "Buy milk and bread.txt", "names: text: its first line");
    expect(textFileName("one two three four five six seven eight nine ten eleven") == "one two three four five six seven eight.txt",
           "names: text: a few words, cut at a space");
    expect(urlFileName("https://user:pw@www.example.com/a?b", "") == "example.com.url", "names: link: its host, no credentials");
    expect(urlFileName("https://example.com", "A: page") == "A_ page.url", "names: link: its title, made safe");
    expect(urlFileBody("https://x.org/\r\nevil") == "[InternetShortcut]\r\nURL=https://x.org/evil\r\n", "names: .url body, one line");
    expect(textFileBody("hi") == "\xEF\xBB\xBFhi", "names: text gets a BOM");
}

void itemsTests()
{
    Items items;
    auto a = items.add({ .name = "a\tb.pdf", .kind = Kind::Link, .path = "C:\\x\\a.pdf" });
    auto b = items.add({ .name = "b", .kind = Kind::Copy, .state = State::Copying, .source = "C:\\b", .bytes = 5 });
    auto c = items.add({ .name = "c", .kind = Kind::Copy, .state = State::Copying, .bytes = 7 });
    expect(items.copied(c, "S:\\3\\c", 8) && items.find(c)->state == State::Ready && items.storeUsed() == 13, "items: a finished copy counts");
    Items back = Items::load(items.save());
    expect(back.all().size() == 3 && back.find(a)->name == "a\tb.pdf" && back.find(a)->path == "C:\\x\\a.pdf", "items: saved and loaded");
    expect(back.find(b)->state == State::Failed && back.find(b)->problem == "The copy didn't finish", "items: a copy under way at quit loads failed");
    expect(back.keepAsLink(b) && back.find(b)->kind == Kind::Link && back.find(b)->path == "C:\\b", "items: ...and can be kept as a link");
    expect(back.add({ .name = "d" }) == 4, "items: ids carry on after a load");
    expect(Items::load("junk\n\n1\tlink").all().empty(), "items: lines not ours are skipped");
    expect(back.setMissing(a, true) && back.find(a)->state == State::Missing && back.setMissing(a, false), "items: a link goes missing and comes back");
}

// A shelf whose hooks record calls; the test answers the copies.
struct Shelf {
    PanelHost host;
    std::vector<CopyJob> jobs;
    std::vector<ShelfHooks::Progress> progress;
    std::vector<ShelfHooks::Done> done;
    std::vector<std::string> removed, opened;
    std::function<void(std::vector<bool>)> checked;
    ShelfProvider provider{ ShelfHooks{
        .copy = [this](const CopyJob& j, ShelfHooks::Progress p, ShelfHooks::Done d) { jobs.push_back(j); progress.push_back(p); done.push_back(d); },
        .removeCopy = [this](const std::string& s) { removed.push_back(s); },
        .open = [this](const std::string& p) { opened.push_back(p); },
        .checkLinks = [this](const std::vector<std::string>&, std::function<void(std::vector<bool>)> d) { checked = std::move(d); } } };
    Shelf() { provider.start(host); }
    void act(const std::string& action) { provider.onAction({ "shelf", kPanelId, action, 0 }); }
    const shelf::Item& item(std::size_t i) const { return provider.items().all()[i]; }
};

DroppedItem dropped(const std::string& path, std::uint64_t mb)
{
    return { .what = file(path.substr(path.rfind('\\') + 1), mb), .path = path };
}

void providerTests()
{
    Shelf s;
    expect(!s.host.published.empty() && s.host.published.back().id == kPanelId, "shelf: its widget on Home");
    auto zones = s.provider.dropZones(drag({ file("a.pdf", 2), file("film", 4300) }));
    expect(zones.size() == 2 && zones[0].id == ShelfProvider::kLinkZone && zones[1].id == ShelfProvider::kCopyZone, "shelf: two zones, Link and Copy");
    expect(zones[0].effect == DropEffect::Link && zones[1].effect == DropEffect::Copy && zones[1].cardId == kPanelId, "shelf: ...never a move; its card opens");
    expect(zones[1].label == "Copy 1 · 1 too large, will be linked", "shelf: ...the zone says what it'll do");
    expect(s.provider.dropZones({}).empty(), "shelf: nothing dragged, no zones");

    s.provider.onDrop(ShelfProvider::kCopyZone, { dropped("C:\\d\\a.pdf", 2), dropped("C:\\d\\film", 4300) });
    expect(s.provider.items().all().size() == 2 && s.item(0).state == State::Copying && s.item(1).kind == Kind::Link,
           "shelf: a mixed drop: one copying, the big one linked");
    expect(s.jobs.size() == 1 && s.jobs[0].source == "C:\\d\\a.pdf" && s.jobs[0].name == "a.pdf" && s.jobs[0].maxBytes == 1024 * kMB,
           "shelf: the copy is asked of the app, with the limit");
    expect(s.host.revealed == std::string(action::kList) + "@0", "shelf: the newest is kept in view");
    s.progress[0](0.5f);
    expect(s.item(0).progress == 0.5f, "shelf: progress shows");
    s.done[0]("S:\\1\\a.pdf", 2 * kMB, "");
    expect(s.item(0).state == State::Ready && s.item(0).path == "S:\\1\\a.pdf", "shelf: the copy is done");
    expect(!s.host.store[ShelfProvider::kItemsKey].empty(), "shelf: saved");

    s.provider.onDrop(ShelfProvider::kCopyZone, { dropped("\\\\nas\\x.iso", 3) });
    s.done[1]("", 0, "The network went away");
    expect(s.item(2).state == State::Failed, "shelf: a copy can fail");
    s.act(std::string(action::kKeep) + std::to_string(s.item(2).id));
    expect(s.item(2).kind == Kind::Link && s.item(2).path == "\\\\nas\\x.iso", "shelf: ...and be kept as a link");

    s.act(std::string(action::kRemove) + std::to_string(s.item(1).id)); // the link to the film
    expect(s.removed.empty() && s.provider.items().all().size() == 2, "shelf: removing a link never touches its original");
    s.act(std::string(action::kRemove) + std::to_string(s.item(0).id));
    expect(s.removed == std::vector<std::string>{ "S:\\1\\a.pdf" }, "shelf: removing a copy deletes it from the store");

    s.act(std::string(action::kOpen) + std::to_string(s.item(0).id));
    expect(s.opened == std::vector<std::string>{ "\\\\nas\\x.iso" }, "shelf: open");

    s.provider.onCardShown(kPanelId);
    s.checked({ false });
    expect(s.item(0).state == State::Missing, "shelf: a link whose original is gone shows it, checked when the card is seen");

    s.act(action::kClear);
    expect(s.host.asked.size() == 1 && s.host.asked[0].message.find("Originals are kept") != std::string::npos && !s.provider.items().all().empty(),
           "shelf: Clear asks first, and says originals stay");
    s.act(action::kClearConfirmed);
    expect(s.provider.items().all().empty(), "shelf: ...cleared once confirmed");

    s.provider.onDrop(ShelfProvider::kLinkZone, { DroppedItem{ .what = text(5), .text = "Hello world" } });
    expect(s.jobs.back().contents == textFileBody("Hello world") && s.jobs.back().name == "Hello world.txt", "shelf: dropped text is written as a .txt");
    s.act(std::string(action::kRemove) + std::to_string(s.item(0).id));
    s.done.back()("S:\\9\\Hello world.txt", 14, "");
    expect(s.removed.back() == "S:\\9\\Hello world.txt", "shelf: a copy that finishes after its item was removed is deleted");

    Shelf restarted;
    restarted.host.store = s.host.store;
    restarted.provider.start(restarted.host);
    expect(restarted.provider.items().all() == s.provider.items().all(), "shelf: the list survives a restart");
}

} // namespace

void runShelfTests()
{
    planTests();
    nameTests();
    itemsTests();
    providerTests();
}

} // namespace cup::test
