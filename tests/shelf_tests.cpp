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
    std::vector<std::string> revealed, renames, pictures;
    std::vector<std::function<void(const std::string&, const std::string&)>> renamed;
    std::vector<std::function<void(std::shared_ptr<const ImageData>)>> pictured;
    ShelfProvider provider{ ShelfHooks{
        .copy = [this](const CopyJob& j, ShelfHooks::Progress p, ShelfHooks::Done d) { jobs.push_back(j); progress.push_back(p); done.push_back(d); },
        .removeCopy = [this](const std::string& s) { removed.push_back(s); },
        .open = [this](const std::string& p) { opened.push_back(p); },
        .reveal = [this](const std::string& p) { revealed.push_back(p); },
        .renameCopy = [this](const std::string& stored, const std::string& name, std::function<void(const std::string&, const std::string&)> d) {
            renames.push_back(stored + ">" + name);
            renamed.push_back(std::move(d));
        },
        .thumbnail = [this](const std::string& path, std::function<void(std::shared_ptr<const ImageData>)> d) {
            pictures.push_back(path);
            pictured.push_back(std::move(d));
        },
        .checkLinks = [this](const std::vector<std::string>&, std::function<void(std::vector<bool>)> d) { checked = std::move(d); } } };
    Shelf() { provider.start(host); }
    void act(const std::string& action) { provider.onAction({ "shelf", kPanelId, action, 0 }); }
    const shelf::Entry& item(std::size_t i) const { return provider.items().all()[i]; }
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
    expect(s.host.revealed == std::string(action::kList) + "@0" && s.host.focused, "shelf: the newest is kept in view, with the keyboard's focus");
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
    expect(s.host.asked.size() == 1 && s.host.asked.back().message.find("original stays") != std::string::npos && s.provider.items().all().size() == 3,
           "shelf: Remove asks first, and says the original stays");
    s.act(std::string(action::kRemoveConfirmed) + std::to_string(s.item(1).id));
    expect(s.removed.empty() && s.provider.items().all().size() == 2, "shelf: removing a link never touches its original");
    s.act(std::string(action::kRemoveConfirmed) + std::to_string(s.item(0).id));
    expect(s.removed == std::vector<std::string>{ "S:\\1\\a.pdf" }, "shelf: removing a copy deletes it from the store");

    s.act(std::string(action::kOpen) + std::to_string(s.item(0).id));
    expect(s.opened == std::vector<std::string>{ "\\\\nas\\x.iso" }, "shelf: open");

    s.provider.onCardShown(kPanelId);
    s.checked({ false });
    expect(s.item(0).state == State::Missing, "shelf: a link whose original is gone shows it, checked when the card is seen");

    s.act(action::kClear);
    expect(s.host.asked.size() == 2 && s.host.asked.back().message.find("Originals are kept") != std::string::npos && !s.provider.items().all().empty(),
           "shelf: Clear asks first, and says originals stay");
    s.act(action::kClearConfirmed);
    expect(s.provider.items().all().empty(), "shelf: ...cleared once confirmed");

    s.provider.onDrop(ShelfProvider::kLinkZone, { DroppedItem{ .what = text(5), .text = "Hello world" } });
    expect(s.jobs.back().contents == textFileBody("Hello world") && s.jobs.back().name == "Hello world.txt", "shelf: dropped text is written as a .txt");
    s.act(std::string(action::kRemoveConfirmed) + std::to_string(s.item(0).id));
    s.done.back()("S:\\9\\Hello world.txt", 14, "");
    expect(s.removed.back() == "S:\\9\\Hello world.txt", "shelf: a copy that finishes after its item was removed is deleted");

    Shelf restarted;
    restarted.host.store = s.host.store;
    restarted.provider.start(restarted.host);
    expect(restarted.provider.items().all() == s.provider.items().all(), "shelf: the list survives a restart");
}

// A published card's grid (cards view), or nothing.
const Row* gridOf(const Panel& p)
{
    for (const Row& r : p.content.expandedCenter)
        if (!r.cells.empty()) return &r;
    return nullptr;
}

bool hasAction(const std::vector<cup::Item>& items, const std::string& prefix)
{
    for (const cup::Item& i : items)
        if (auto* b = std::get_if<Button>(&i); b && b->actionId.starts_with(prefix)) return true;
    return false;
}

void cardTests()
{
    Shelf s;
    expect(s.provider.view() == View::Cards && s.host.published.back().content.card == CardLayout::Large, "card: Large, cards by default");
    expect(s.host.published.back().content.steadyPress, "card: a press doesn't squash the island");
    expect(s.host.published.back().content.tileLeading[0].items[0] == cup::Item{ Icon{ Glyph::Package } }, "card: the widget's icon is a box");
    s.provider.onDrop(ShelfProvider::kLinkZone, { dropped("C:\\d\\a.pdf", 2), DroppedItem{ .what = folder("Photos"), .path = "C:\\Photos" } });
    s.provider.onDrop(ShelfProvider::kCopyZone, { dropped("C:\\d\\b.png", 1) });
    const Row* grid = gridOf(s.host.published.back());
    const Slot& page = s.host.published.back().content.expandedCenter;
    auto* summary = std::get_if<Text>(&page.front().items[1]);
    expect(summary && summary->value == "3 things · 3 MB", "card: how many, and their total size");
    auto* clear = std::get_if<Button>(&page.back().items[0]);
    expect(clear && clear->actionId == action::kClear && clear->withGlyph && clear->glyph == Glyph::Delete, "card: Clear at the foot, with its icon");
    expect(grid && grid->cells.size() == 4 && grid->list == action::kList && grid->selectable, "card: one cell per thing, in a scrolling grid, picked like files");
    expect(grid && grid->cells.back().fixed && hasAction(grid->cells.back().rows[0].items, action::kAdd), "card: an add card last, never picked");
    if (grid) {
        const Cell& copying = grid->cells[0]; // the newest first
        const Cell& link = grid->cells[2];
        expect(copying.openAction.empty() && !hasAction(copying.actions, action::kReveal), "card: a copy under way can't be opened or shown yet");
        expect(link.openAction == action::kOpen + std::to_string(s.item(0).id) && hasAction(link.actions, action::kReveal) &&
               hasAction(link.actions, action::kRemove), "card: a ready thing opens on a double-click, and shows Show in folder and Remove");
        bool hover = true;
        for (const cup::Item& a : link.actions) hover = hover && std::get<Button>(a).revealOnHover;
        expect(hover, "card: its buttons show only on hover");
        auto* name = std::get_if<Text>(&link.rows[1].items[0]);
        expect(name && name->value == "a.pdf" && name->editAction == action::kRename + std::to_string(s.item(0).id) && name->editHint,
               "card: the name renames it, and lights to say so");
        auto* kind = std::get_if<Text>(&link.rows[2].items[1]);
        expect(link.rows[2].items[0] == cup::Item{ Icon{ .glyph = Glyph::Link, .compact = true } } && kind && kind->value == "Link · 2 MB",
               "card: a small badge by what it is");
        auto* steps = std::get_if<Button>(&link.actions.back());
        expect(steps && steps->backed, "card: its buttons have a backdrop, readable over any picture");
    }

    s.act(action::kView);
    expect(s.provider.view() == View::List && s.host.store[ShelfProvider::kViewKey] == "list" && !gridOf(s.host.published.back()),
           "card: the toggle switches to the list, and it's kept");
    const Row* folderRow = nullptr;
    for (const Row& r : s.host.published.back().content.expandedCenter)
        if (r.id == "item-" + std::to_string(s.item(1).id)) folderRow = &r;
    expect(folderRow && folderRow->items[0] == cup::Item{ Icon{ Glyph::Folder } } && folderRow->openAction.starts_with(action::kOpen),
           "card: a folder has the folder icon, and opens on a double-click");
    bool hover = folderRow != nullptr;
    if (folderRow)
        for (const cup::Item& a : folderRow->trailing) hover = hover && std::get<Button>(a).revealOnHover;
    expect(hover, "card: in the list too, buttons only on hover");
    Shelf again;
    again.host.store = s.host.store;
    again.provider.start(again.host);
    expect(again.provider.view() == View::List, "card: the view survives a restart");

    s.act(std::string(action::kReveal) + std::to_string(s.item(0).id));
    s.provider.onAction({ "shelf", kPanelId, std::string(action::kOpen) + std::to_string(s.item(1).id), 0 });
    expect(s.revealed == std::vector<std::string>{ "C:\\d\\a.pdf" } && s.opened == std::vector<std::string>{ "C:\\Photos" },
           "card: Show in folder reveals it; a double-click opens it");

    // Renaming: a link only on the shelf; a copy's file in the store.
    s.provider.onAction({ "shelf", kPanelId, std::string(action::kRename) + std::to_string(s.item(0).id), 0, "  Taxes 2026 " });
    expect(shownName(s.item(0)) == "Taxes 2026" && s.item(0).name == "a.pdf" && s.item(0).path == "C:\\d\\a.pdf" && s.renames.empty(),
           "card: renaming a link changes its label only");
    expect(Items::load(s.provider.items().save()).find(s.item(0).id)->label == "Taxes 2026", "card: the label is kept");
    s.done[0]("S:\\3\\b.png", 1 * kMB, "");
    std::uint64_t copy = s.item(2).id;
    s.provider.onAction({ "shelf", kPanelId, std::string(action::kRename) + std::to_string(copy), 0, "c:d.png" });
    expect(s.renames == std::vector<std::string>{ "S:\\3\\b.png>c_d.png" }, "card: renaming a copy renames its file, made safe");
    s.renamed[0]("S:\\3\\c_d.png", "");
    expect(s.item(2).path == "S:\\3\\c_d.png" && s.item(2).name == "c_d.png", "card: ...and it's found at its new name");
    s.provider.onAction({ "shelf", kPanelId, std::string(action::kRename) + std::to_string(copy), 0, "x.partial" });
    expect(s.renames.size() == 1, "card: never a name the next start would delete");

    // Pictures: read once the card is seen, shown, forgotten with their thing.
    expect(s.pictures.empty(), "card: no pictures read before the card is seen");
    s.provider.onCardShown(kPanelId);
    expect(s.pictures.size() == 3, "card: each ready thing's picture is asked for once seen");
    auto image = std::make_shared<ImageData>(ImageData{ 1, 1, { 0, 0, 0, 255 } });
    s.pictured[0](image);
    s.act(action::kView); // back to cards
    const Row* shown = gridOf(s.host.published.back());
    bool pictured = false;
    if (shown)
        for (const Cell& c : shown->cells)
            if (auto* img = std::get_if<Image>(&c.rows[0].items[0]); img && img->data == image) pictured = img->role == ImageRole::Thumbnail;
    expect(pictured, "card: the picture is the cell's thumbnail");
    s.provider.onCardShown(kPanelId);
    expect(s.pictures.size() == 3, "card: a picture is asked for once");

    expect(Items::load("1\tlink\tready\tfile\t0\ta\tC:\\a\t\t\n").all().size() == 1, "card: a list saved before labels still loads");
}

void selectionTests()
{
    Shelf s;
    s.provider.onDrop(ShelfProvider::kLinkZone, { dropped("C:\\a", 1), dropped("C:\\b", 1), dropped("C:\\c", 1) });
    auto foot = [&] { return std::get<Button>(s.host.published.back().content.expandedCenter.back().items[0]); };
    expect(foot().actionId == action::kClear, "picked: nothing picked, the foot clears");
    std::string a = "item-" + std::to_string(s.item(0).id), c = "item-" + std::to_string(s.item(2).id);
    s.provider.onSelectionChanged(kPanelId, action::kList, { c, a });
    expect(foot().label == "Delete 2 items" && foot().actionId == action::kDeleteSelected && foot().destructive, "picked: the foot deletes them");
    s.act(action::kDeleteSelected);
    expect(!s.host.asked.empty() && s.host.asked.back().title == "Delete 2 items?" && s.provider.items().all().size() == 3, "picked: Delete asks first");
    s.act(action::kDeleteSelectedConfirmed);
    expect(s.provider.items().all().size() == 1 && s.item(0).path == "C:\\b" && foot().actionId == action::kClear, "picked: ...then they go");
    expect(s.host.published.back().content.deleteAction == action::kDeleteSelected, "picked: the Delete key does the same");

    // The add card: Windows' picker, kept as copies; what can't be copied is linked, and a toast says so.
    std::function<void(std::vector<DroppedItem>)> picked;
    Shelf p;
    ShelfProvider adder{ ShelfHooks{ .copy = [&](const CopyJob&, ShelfHooks::Progress, ShelfHooks::Done) {},
                                      .pickFiles = [&](std::function<void(std::vector<DroppedItem>)> d) { picked = std::move(d); } } };
    adder.start(p.host);
    adder.onAction({ "shelf", kPanelId, action::kAdd, 0 });
    expect(static_cast<bool>(picked), "add: the picker opens");
    picked({ dropped("C:\\small.pdf", 1), dropped("C:\\film.mkv", 4300) });
    expect(adder.items().all().size() == 2 && adder.items().all()[0].kind == Kind::Copy && adder.items().all()[1].kind == Kind::Link,
           "add: copies, as the Copy zone would; too large ones linked");
    expect(p.host.alerts.size() == 1 && p.host.alerts[0].content.label == "1 file was added as a link", "add: a toast says what was linked");
}

} // namespace

void runShelfTests()
{
    planTests();
    nameTests();
    itemsTests();
    providerTests();
    cardTests();
    selectionTests();
}

} // namespace cup::test
