// Settings' Tab row (decision 048), Music colours (decisions 052, 053), Glass (decision 089), and "New" on settings not seen yet: a
// tag on the row, a dot on its parent's while it's on a page not opened, and
// seen only once the card closes after showing it.

#include "expect.h"
#include "panel_host.h"
#include "core/hit_test.h"
#include "core/keyboard.h"
#include "core/layout.h"
#include "core/segments.h"
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
    // The setting: under Appearance, Top and Bottom picked in place (decision 093); picking moves the row at once.
    PanelHost h;
    h.store[SettingsProvider::kTabRowKey] = "bottom";
    h.store[SettingsProvider::kSeenKey] = "seen1\ntabrow\n"; // seen already: no tag between its name and value
    std::vector<bool> placed;
    SettingsProvider s({ .placeTabRow = [&](bool top) { placed.push_back(top); } });
    s.start(h);
    expect(placed == std::vector<bool>{ false }, "tab row: the saved place is applied at start");
    auto segments = [&] { return std::get<Segments>(rowNamed(h.published.back(), "Tab row")->items.back()); };
    expect(segments().options == std::vector<std::string>{ "Top", "Bottom" } && segments().picked == 1 &&
           !rowNamed(h.published.back(), "Tab row")->clickAnywhere,
           "tab row: a row with Top and Bottom side by side, Bottom picked; the row itself isn't one button");
    s.onAction({ "system.settings", SettingsProvider::kPanelId, segmentAction(segments(), 0), 0 });
    expect(placed.back() && h.store[SettingsProvider::kTabRowKey] == "top" && segments().picked == 0 &&
           h.published.back().content.expandedCenter[0].leading.empty(),
           "tab row: picked Top, applied and kept, on the list itself: no page");

    // Laid out: the options side by side; a click on one picks it, Enter on the control the next one.
    {
        Panel panel = h.published.back();
        panel.providerId = "system.settings";
        IslandState st;
        st = reduce(st, PanelPublished{ panel }, Ms{ 0 });
        st = reduce(st, HomeOpened{}, Ms{ 0 });
        st = reduce(st, TileClicked{ kSettingsPanelId }, Ms{ 0 });
        Scene scene = layout(st, Ms{ 0 });
        const Placed* control = nullptr;
        for (const Placed& e : scene.elements)
            if (std::holds_alternative<Segments>(e.item)) control = &e;
        Rect bottom = control ? segmentFrame(segments(), control->frame, 1) : Rect{};
        auto hit = control ? hitTest(scene, { bottom.x + bottom.width / 2, bottom.y + bottom.height / 2 }) : std::nullopt;
        expect(control && control->frame.height == tokens().segmentHeight && hit && hit->action.actionId == std::string(P::kPickTabRow) + "1",
               "segments: a click on Bottom sends its pick");
        auto f = control ? findFocusable(scene, "el:" + control->key) : std::nullopt;
        auto pressed = f ? hitTest(scene, f->point) : std::nullopt;
        expect(pressed && pressed->action.actionId == std::string(P::kPickTabRow) + "1", "segments: Enter on Top picks the next one, Bottom");
    }

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
    std::vector<SettingItem> tabRowOnly{ { "tabrow", "" } }; // no app hooks: Colours from music has no row to be seen on
    SettingsProvider s({ .announced = tabRowOnly });
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
    SettingsProvider later({ .announced = tabRowOnly });
    later.start(h);
    expect(!tagged(rowNamed(h.published.back(), "Tab row")), "new: and it stays gone after a restart");

    // A new subitem: a dot on its parent's row, the tag on the item itself.
    PanelHost sub;
    sub.store[SettingsProvider::kSeenKey] = "seen1\nmusiccolours\n";
    std::vector<SettingItem> announced{ { "musiccolours", "" }, { "covercolours", "musiccolours" } };
    SettingsProvider n({ .setCoverColours = [](bool) {}, .announced = announced });
    n.start(sub);
    auto list = [&] { return sub.published.back(); };
    expect(dotted(rowNamed(list(), "Music colours")) && !tagged(rowNamed(list(), "Music colours")), "new subitem: a dot on its parent, no tag there");
    n.onCardShown(SettingsProvider::kPanelId);
    n.onCardClosed(SettingsProvider::kPanelId);
    expect(dotted(rowNamed(list(), "Music colours")), "new subitem: its page not opened, re-entering keeps the dot");
    n.onCardShown(SettingsProvider::kPanelId);
    n.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenMusicColours, 0 });
    expect(tagged(rowNamed(list(), "Colours from music")), "new subitem: on its page, tagged New");
    n.onCardClosed(SettingsProvider::kPanelId);
    expect(!dotted(rowNamed(list(), "Music colours")), "new subitem: seen, the parent's dot is gone");
    n.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenMusicColours, 0 });
    expect(!tagged(rowNamed(list(), "Colours from music")), "new subitem: and so is its tag");

    // Laid out: the tag sits right after the name, not pushed to the row's end.
    Panel tagPanel = settingsPanel({ .page = SettingsPage::MusicColours, .coverColours = false, .announced = announced, .unseen = { "covercolours" } });
    tagPanel.providerId = "system.settings";
    IslandState st;
    st = reduce(st, PanelPublished{ tagPanel }, Ms{ 0 });
    st = reduce(st, HomeOpened{}, Ms{ 0 });
    st = reduce(st, TileClicked{ kSettingsPanelId }, Ms{ 0 });
    Scene scene = layout(st, Ms{ 0 }, [](const Text& text) { return 8.0f * static_cast<float>(text.value.size()); });
    const Placed* name = nullptr;
    const Placed* tag = nullptr;
    for (const Placed& p : scene.elements) {
        if (auto* x = std::get_if<Text>(&p.item); x && x->value == "Colours from music") name = &p;
        if (std::holds_alternative<Tag>(p.item)) tag = &p;
    }
    expect(name && tag && std::abs(tag->frame.x - (name->frame.x + name->frame.width + tokens().itemSpacing)) < 0.01f &&
           tag->frame.height == tokens().tagHeight, "new: the tag right after the name");
}

// A tagged setting's name and description (under it, decision 090) stay where an
// untagged one's are, and its control at the row's end.
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
    auto value = [](const Placed& e) { auto* x = std::get_if<Text>(&e.item); return x && x->value == "Tab row"; };
    auto chevron = [](const Placed& e) { return std::holds_alternative<Segments>(e.item); };
    const Placed* plainValue = find(rowOf[0], value);
    const Placed* taggedValue = find(rowOf[1], value);
    const Placed* plainChevron = find(rowOf[0], chevron);
    const Placed* taggedChevron = find(rowOf[1], chevron);
    expect(plainValue && taggedValue && taggedValue->frame.width > 0 && taggedValue->frame.x == plainValue->frame.x,
           "new: the tagged row's name and value start where an untagged one's do");
    expect(plainChevron && taggedChevron && taggedChevron->frame.x == plainChevron->frame.x, "new: and its chevron stays at the edge");
}

// Music colours (decisions 052, 053): one page with Colours from music, a
// switch that's on until turned off, and the colour fade's speed, medium by default.
void coverColoursTests()
{
    auto switchedOn = [](const Row* r) {
        auto* toggle = r && r->clickAnywhere && r->items.size() > 1 ? std::get_if<Switch>(&r->items.back()) : nullptr;
        return toggle && toggle->on;
    };
    auto value = [](const Row* r) { return r ? std::get<Text>(r->items[0]).detail : std::string(); }; // under its name (decision 090)
    PanelHost h;
    std::vector<bool> applied;
    std::vector<FadeSpeed> speeds;
    SettingsProvider settings({ .setCoverColours = [&](bool on) { applied.push_back(on); },
                                .setFadeSpeed = [&](FadeSpeed s) { speeds.push_back(s); } });
    settings.start(h);
    expect(applied == std::vector<bool>{ false } && speeds == std::vector<FadeSpeed>{ FadeSpeed::Medium },
           "music colours: off and medium by default (decision 055), and the app told");
    expect(value(rowNamed(h.published.back(), "Music colours")) == "Off" && !rowNamed(h.published.back(), "Colours from music"),
           "music colours: one row on the list, saying it's off; the switch is on its page");
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenMusicColours, 0 });
    expect(rowNamed(h.published.back(), "Colours from music") && !switchedOn(rowNamed(h.published.back(), "Colours from music")),
           "music colours: its page, the switch off");
    auto fade = [&] { return std::get<Segments>(rowNamed(h.published.back(), "Colour fade")->items.back()); };
    expect(fade().options == std::vector<std::string>{ "Fast", "Medium", "Slow" } && fade().picked == 1,
           "music colours: the fade's three speeds side by side, medium picked (decision 093)");

    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kToggleCoverColours, 0 });
    expect(applied.back() == true && h.store[SettingsProvider::kCoverColoursKey] == "on" &&
               switchedOn(rowNamed(h.published.back(), "Colours from music")),
           "music colours: a click switches it on, saved, the page stays");
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, std::string(P::kPickFade) + "2", 0 });
    expect(speeds.back() == FadeSpeed::Slow && h.store[SettingsProvider::kFadeKey] == "slow" &&
               fade().picked == 2,
           "music colours: slow picked, saved, ticked");
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kBack, 0 });
    expect(value(rowNamed(h.published.back(), "Music colours")) == "On", "music colours: the list says on");
    settings.stop();
    settings.start(h);
    expect(applied.back() == true && speeds.back() == FadeSpeed::Slow, "music colours: both kept over a restart");
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kToggleCoverColours, 0 });
    expect(applied.back() == false && h.store[SettingsProvider::kCoverColoursKey] == "off", "music colours: and back off");
    settings.stop();

    PanelHost bare;
    SettingsProvider without({});
    without.start(bare);
    expect(!rowNamed(bare.published.back(), "Music colours"), "music colours: no hook, no row");
    without.stop();
}

// Glass (decision 089): a switch on the list, on until turned off (after an
// update too), and only where the app can draw it (Windows 11): no hook, no row.
void glassTests()
{
    auto switchedOn = [](const Row* r) {
        auto* toggle = r && r->clickAnywhere && r->items.size() > 1 ? std::get_if<Switch>(&r->items.back()) : nullptr;
        return toggle && toggle->on;
    };
    PanelHost h;
    std::vector<bool> applied;
    SettingsProvider settings({ .setGlass = [&](bool on) { applied.push_back(on); } });
    settings.start(h);
    expect(applied == std::vector<bool>{ true } && h.store[SettingsProvider::kGlassKey].empty() && switchedOn(rowNamed(h.published.back(), "Glass")),
           "glass: on by default (nothing saved, as after an update), the app told, a switch on the list");
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kToggleGlass, 0 });
    expect(applied.back() == false && h.store[SettingsProvider::kGlassKey] == "off" && !switchedOn(rowNamed(h.published.back(), "Glass")),
           "glass: a click turns it off, saved");
    settings.stop();
    settings.start(h);
    expect(applied.back() == false && !switchedOn(rowNamed(h.published.back(), "Glass")), "glass: off kept over a restart");
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kToggleGlass, 0 });
    expect(applied.back() == true && h.store[SettingsProvider::kGlassKey] == "on", "glass: and back on");
    settings.stop();

    PanelHost bare;
    SettingsProvider without({});
    without.start(bare);
    without.onAction({ "system.settings", SettingsProvider::kPanelId, P::kToggleGlass, 0 });
    expect(!rowNamed(bare.published.back(), "Glass") && bare.store[SettingsProvider::kGlassKey].empty(),
           "glass: no hook (Windows 10), no row, and its action does nothing");
    without.stop();
}

} // namespace

void runSettingsNewTests()
{
    tabRowTests();
    coverColoursTests();
    glassTests();
    newTagTests();
    tagKeepsTheEnd();
}

} // namespace cup::test
