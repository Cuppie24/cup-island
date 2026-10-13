// Settings' Tab row (decision 048), and "New" on settings not seen yet: a
// tag on the row, a dot on its parent's while it's on a page not opened, and
// seen only once the card closes after showing it.

#include "expect.h"
#include "panel_host.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "providers/system/settings_provider.h"

#include <algorithm>

namespace cup::test {
namespace {

namespace P = settings_action;

bool tagged(const Row* r)
{
    return r && std::any_of(r->items.begin(), r->items.end(), [](const Item& item) {
        auto* tag = std::get_if<Tag>(&item);
        return tag && tag->text == "New";
    });
}

bool dotted(const Row* r)
{
    return r && std::any_of(r->items.begin(), r->items.end(), [](const Item& item) {
        auto* b = std::get_if<Button>(&item);
        return b && b->badge;
    });
}

void tabRowTests()
{
    // The setting: under Appearance, a page of Top and Bottom; picking moves the row at once.
    PanelHost h;
    h.store[SettingsProvider::kTabRowKey] = "bottom";
    h.store[SettingsProvider::kSeenKey] = "seen1\ntabrow\n"; // seen already: no tag between its name and value
    std::vector<bool> placed;
    SettingsProvider s({ .placeTabRow = [&](bool top) { placed.push_back(top); } });
    s.start(h);
    expect(placed == std::vector<bool>{ false }, "tab row: the saved place is applied at start");
    expect(valueOf(h.published.back(), "Tab row") == "Bottom", "tab row: a row that says where it is");
    s.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenTabRow, 0 });
    const Panel& page = h.published.back();
    expect(cardText(page, 1) == "Top" && cardText(page, 2) == "Bottom", "tab row: its page, Top and Bottom");
    s.onAction({ "system.settings", SettingsProvider::kPanelId, std::string(P::kPickTabRow) + "0", 0 });
    expect(placed.back() && h.store[SettingsProvider::kTabRowKey] == "top" && cardText(h.published.back(), 1) == "Top",
           "tab row: picked Top, applied and kept; the page stays");

    // The core side: the strip along the bottom, the content where it is without one.
    IslandState top, bottom;
    for (IslandState* st : { &top, &bottom }) {
        *st = reduce(*st, ActivityStarted{ { .id = "a", .content = { .expandedCenter = { row({ Text{ "x" } }) } } } }, Ms{ 0 });
        *st = reduce(*st, ActivityStarted{ { .id = "b", .content = { .expandedCenter = { row({ Text{ "y" } }) } } } }, Ms{ 0 });
        *st = reduce(*st, PillClicked{}, Ms{ 0 });
    }
    bottom = reduce(bottom, TabRowPlaced{ false }, Ms{ 0 });
    Scene up = layout(top, Ms{ 0 }), down = layout(bottom, Ms{ 0 });
    const Tokens& t = tokens();
    expect(up.tabs->tabs[0].frame.y == t.expandedPadding && down.tabs->tabs[0].frame.y == down.size.y - t.expandedPadding - t.tabHeight,
           "tab row: along the top, or the bottom");
    expect(down.size == up.size, "tab row: the card is as tall either way");
    float upText = 0, downText = 0;
    for (const Placed& p : up.elements) if (p.key.rfind("tab/", 0) != 0) upText = p.frame.y;
    for (const Placed& p : down.elements) if (p.key.rfind("tab/", 0) != 0) downText = p.frame.y;
    expect(std::abs(upText - downText - (t.tabHeight + t.slotGap)) < 0.01f, "tab row: the content above a bottom strip, under a top one");
    expect(down.tabs->pin && down.tabs->pin->hitFrame.y + down.tabs->pin->hitFrame.height == down.size.y, "tab row: at the bottom, the pin reaches the card's bottom corner");
}

void newTagTests()
{
    // A fresh install: nothing is new.
    PanelHost fresh;
    SettingsProvider first({});
    first.start(fresh);
    expect(!tagged(rowNamed(fresh.published.back(), "Tab row")) && !fresh.published.back().content.badge, "new: a fresh install tags nothing");
    expect(fresh.store[SettingsProvider::kSeenKey].rfind("seen1\n", 0) == 0, "new: ...and remembers it has seen them");

    // An update from before tags (other settings kept): the announced one is new.
    PanelHost h;
    h.store[SettingsProvider::kAutostartKey] = "on";
    SettingsProvider s({});
    s.start(h);
    auto card = [&] { return h.published.back(); };
    expect(tagged(rowNamed(card(), "Tab row")) && !tagged(rowNamed(card(), "Copy logs")), "new: the new setting tagged, the others not");
    expect(card().content.badge, "new: a dot on the Settings widget and its tab");
    s.onCardClosed(SettingsProvider::kPanelId); // closed without ever being shown (it was only published)
    expect(tagged(rowNamed(card(), "Tab row")), "new: not seen until its card was on screen");
    s.onCardShown(SettingsProvider::kPanelId);
    expect(tagged(rowNamed(card(), "Tab row")), "new: still tagged while being seen");
    s.onCardClosed(SettingsProvider::kPanelId);
    expect(!tagged(rowNamed(card(), "Tab row")) && !card().content.badge, "new: re-entering, the tag is gone");
    SettingsProvider later({});
    later.start(h);
    expect(!tagged(rowNamed(h.published.back(), "Tab row")), "new: and it stays gone after a restart");

    // A new subitem: a dot on its parent's row, the tag on the item itself.
    PanelHost sub;
    sub.store[SettingsProvider::kSeenKey] = "seen1\ntabrow\n";
    std::vector<SettingItem> announced{ { "tabrow", "" }, { "tabrow.bottom", "tabrow" } };
    SettingsProvider n({ .announced = announced });
    n.start(sub);
    auto list = [&] { return sub.published.back(); };
    expect(dotted(rowNamed(list(), "Tab row")) && !tagged(rowNamed(list(), "Tab row")), "new subitem: a dot on its parent, no tag there");
    n.onCardShown(SettingsProvider::kPanelId);
    n.onCardClosed(SettingsProvider::kPanelId);
    expect(dotted(rowNamed(list(), "Tab row")), "new subitem: its page not opened, re-entering keeps the dot");
    n.onCardShown(SettingsProvider::kPanelId);
    n.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenTabRow, 0 });
    expect(tagged(rowNamed(list(), "Bottom")) && !tagged(rowNamed(list(), "Top")), "new subitem: on its page, tagged New");
    n.onCardClosed(SettingsProvider::kPanelId);
    expect(!dotted(rowNamed(list(), "Tab row")), "new subitem: seen, the parent's dot is gone");
    n.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenTabRow, 0 });
    expect(!tagged(rowNamed(list(), "Bottom")), "new subitem: and so is its tag");

    // Laid out: the tag sits right after the name, not pushed to the row's end.
    Panel tagPanel = settingsPanel({ .page = SettingsPage::TabRow, .announced = announced, .unseen = { "tabrow.bottom" } });
    tagPanel.providerId = "system.settings";
    IslandState st;
    st = reduce(st, PanelPublished{ tagPanel }, Ms{ 0 });
    st = reduce(st, HomeOpened{}, Ms{ 0 });
    st = reduce(st, TileClicked{ kSettingsPanelId }, Ms{ 0 });
    Scene scene = layout(st, Ms{ 0 }, [](const Text& text) { return 8.0f * static_cast<float>(text.value.size()); });
    const Placed* name = nullptr;
    const Placed* tag = nullptr;
    for (const Placed& p : scene.elements) {
        if (auto* x = std::get_if<Text>(&p.item); x && x->value == "Bottom") name = &p;
        if (std::holds_alternative<Tag>(p.item)) tag = &p;
    }
    expect(name && tag && std::abs(tag->frame.x - (name->frame.x + name->frame.width + tokens().itemSpacing)) < 0.01f &&
           tag->frame.height == tokens().tagHeight, "new: the tag right after the name");
}

// A tagged setting's value and chevron stay at the row's end, where an untagged one's are.
void tagKeepsTheEnd()
{
    std::vector<Placed> rowOf[2];
    for (int tagged = 0; tagged < 2; ++tagged) {
        SettingsView v{ .announced = { { "tabrow", "" } } };
        if (tagged) v.unseen = { "tabrow" };
        Panel p = settingsPanel(v);
        p.providerId = "system.settings";
        IslandState st;
        st = reduce(st, PanelPublished{ p }, Ms{ 0 });
        st = reduce(st, HomeOpened{}, Ms{ 0 });
        st = reduce(st, TileClicked{ kSettingsPanelId }, Ms{ 0 });
        Scene s = layout(st, Ms{ 0 }, [](const Text& text) { return 8.0f * static_cast<float>(text.value.size()); });
        for (const Placed& e : s.elements)
            if (e.key.find("/tabrow/") != std::string::npos) rowOf[tagged].push_back(e);
    }
    auto find = [](const std::vector<Placed>& row, auto pick) -> const Placed* {
        for (const Placed& e : row) if (pick(e)) return &e;
        return nullptr;
    };
    auto value = [](const Placed& e) { auto* x = std::get_if<Text>(&e.item); return x && x->value == "Top"; };
    auto chevron = [](const Placed& e) { return std::holds_alternative<Button>(e.item); };
    const Placed* plainValue = find(rowOf[0], value);
    const Placed* taggedValue = find(rowOf[1], value);
    const Placed* plainChevron = find(rowOf[0], chevron);
    const Placed* taggedChevron = find(rowOf[1], chevron);
    expect(plainValue && taggedValue && taggedValue->frame.width > 0 &&
           taggedValue->frame.x + taggedValue->frame.width == plainValue->frame.x + plainValue->frame.width,
           "new: the tagged row's value still ends at the row's end");
    expect(plainChevron && taggedChevron && taggedChevron->frame.x == plainChevron->frame.x, "new: and its chevron stays at the edge");
}

} // namespace

void runSettingsNewTests()
{
    tabRowTests();
    newTagTests();
    tagKeepsTheEnd();
}

} // namespace cup::test
