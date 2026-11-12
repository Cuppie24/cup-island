// Settings' Tab row (decisions 048, 090), Music colours (decisions 052, 053),
// and "New" on settings not seen yet: a tag on the row, a dot on its parent's
// while it's on a page not opened, a dot on its page in the sidebar, and seen
// only once the window closes after showing it.

#include "expect.h"
#include "panel_host.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "core/window_layout.h"
#include "providers/system/settings_provider.h"

#include <algorithm>
#include <cmath>

namespace cup::test {
namespace {

namespace P = settings_action;

Action fromWindow(const std::string& actionId, float value = 0, const std::string& text = "")
{
    return { "system.settings", "", actionId, value, text, kSettingsWindowId };
}

void openWindow(SettingsProvider& s) { s.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 }); }

bool tagged(const Row* r)
{
    auto* tag = itemOf<Tag>(r);
    return tag && tag->text == "New";
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
    // The setting: on Appearance, Top and Bottom side by side; a pick moves the row at once.
    PanelHost h;
    h.store[SettingsProvider::kTabRowKey] = "bottom";
    h.store[SettingsProvider::kSeenKey] = "seen1\ntabrow\n";
    std::vector<bool> placed;
    SettingsProvider s({ .placeTabRow = [&](bool top) { placed.push_back(top); } });
    s.start(h);
    expect(placed == std::vector<bool>{ false }, "tab row: the saved place is applied at start");
    openWindow(s);
    const Segmented* choice = itemOf<Segmented>(windowRow(h, "Tab row"));
    expect(choice && choice->options == std::vector<std::string>{ "Top", "Bottom" } && choice->picked == 1 && choice->actionId == P::kSetTabRow &&
           detailOf(h, "Tab row") == "Home and pin buttons",
           "tab row: a row with Top and Bottom side by side (decision 090), Bottom picked");
    s.onAction(fromWindow(P::kSetTabRow, 0));
    expect(placed.back() && h.store[SettingsProvider::kTabRowKey] == "top" && itemOf<Segmented>(windowRow(h, "Tab row"))->picked == 0,
           "tab row: Top picked, applied and kept");

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
    openWindow(first);
    expect(!tagged(windowRow(fresh, "Tab row")) && !fresh.published.back().content.badge, "new: a fresh install tags nothing");
    expect(fresh.store[SettingsProvider::kSeenKey].rfind("seen1\n", 0) == 0, "new: ...and remembers it has seen them");

    // An update from before tags (other settings kept): the announced one is new.
    PanelHost h;
    h.store[SettingsProvider::kAutostartKey] = "on";
    std::vector<SettingItem> tabRowOnly{ { "tabrow", "" } };
    SettingsProvider s({ .announced = tabRowOnly });
    s.start(h);
    expect(h.published.back().content.badge, "new: a dot on the Settings widget");
    openWindow(s);
    expect(tagged(windowRow(h, "Tab row")) && !tagged(windowRow(h, "Copy logs")), "new: the new setting tagged, the others not");
    expect(pageOf(h, kAppearancePage)->badge && !pageOf(h, kGeneralPage)->badge, "new: a dot on its page in the sidebar");
    expect(tagged(windowRow(h, "Tab row")), "new: still tagged while being seen");
    s.onWindowClosed(kSettingsWindowId);
    expect(!h.published.back().content.badge, "new: the window closed after showing it: seen, the widget's dot gone");
    openWindow(s);
    expect(!tagged(windowRow(h, "Tab row")) && !pageOf(h, kAppearancePage)->badge, "new: reopened, the tag is gone");
    SettingsProvider later({ .announced = tabRowOnly });
    later.start(h);
    openWindow(later);
    expect(!tagged(windowRow(h, "Tab row")), "new: and it stays gone after a restart");

    // Only what the shown page has is seen: a new General setting, the window on Appearance.
    PanelHost other;
    other.store[SettingsProvider::kAutostartKey] = "on";
    SettingsProvider o({ .announced = { { "logs", "" } } });
    o.start(other);
    openWindow(o);
    o.onWindowClosed(kSettingsWindowId);
    expect(other.published.back().content.badge, "new: closed without its page shown, still new");
    openWindow(o);
    o.onAction(fromWindow(kWindowPage, 0, kGeneralPage));
    o.onWindowClosed(kSettingsWindowId);
    expect(!other.published.back().content.badge, "new: its page shown, then closed: seen");

    // A new subitem: a dot on its parent's row, the tag on the item itself.
    PanelHost sub;
    sub.store[SettingsProvider::kSeenKey] = "seen1\nmusiccolours\nfade\n";
    std::vector<SettingItem> announced{ { "musiccolours", "" }, { "covercolours", "musiccolours" }, { "fade", "musiccolours" } };
    SettingsProvider n({ .setCoverColours = [](bool) {}, .setFadeSpeed = [](FadeSpeed) {}, .announced = announced });
    n.start(sub);
    openWindow(n);
    expect(dotted(windowRow(sub, "Music colours")) && !tagged(windowRow(sub, "Music colours")), "new subitem: a dot on its parent, no tag there");
    n.onWindowClosed(kSettingsWindowId);
    openWindow(n);
    expect(dotted(windowRow(sub, "Music colours")), "new subitem: its page not opened, reopening keeps the dot");
    n.onAction(fromWindow(P::kOpenMusicColours));
    expect(tagged(windowRow(sub, "Colours from music")), "new subitem: on its page, tagged New");
    n.onWindowClosed(kSettingsWindowId);
    openWindow(n);
    expect(!dotted(windowRow(sub, "Music colours")), "new subitem: seen, the parent's dot is gone");
    n.onAction(fromWindow(P::kOpenMusicColours));
    expect(!tagged(windowRow(sub, "Colours from music")), "new subitem: and so is its tag");
}

// Laid out: the tag sits right after the name; the row's chevron stays at its end.
void tagLayout()
{
    auto measure = [](const Text& text) { return 8.0f * static_cast<float>(text.value.size()); };
    std::vector<Placed> rowOf[2];
    for (int tagged = 0; tagged < 2; ++tagged) {
        SettingsView v{ .coverColours = true, .announced = { { "musiccolours", "" } } };
        if (tagged) v.unseen = { "musiccolours" };
        OpenWindow w{ .key = { "system.settings", kSettingsWindowId }, .content = settingsWindow(v), .page = kAppearancePage };
        WindowScene scene = windowLayout(w, { 900, 700 }, 0, Ms{ 0 }, measure);
        for (const Placed& e : scene.page)
            if (e.key.find("/musiccolours/") != std::string::npos) rowOf[tagged].push_back(e);
    }
    auto find = [](const std::vector<Placed>& row, auto pick) -> const Placed* {
        for (const Placed& e : row) if (pick(e)) return &e;
        return nullptr;
    };
    auto name = [](const Placed& e) { auto* x = std::get_if<Text>(&e.item); return x && x->value == "Music colours"; };
    auto tag = [](const Placed& e) { return std::holds_alternative<Tag>(e.item); };
    auto chevron = [](const Placed& e) { return std::holds_alternative<Button>(e.item); };
    const Placed* n = find(rowOf[1], name);
    const Placed* t = find(rowOf[1], tag);
    expect(n && t && std::abs(t->frame.x - (n->frame.x + n->frame.width + tokens().itemSpacing)) < 0.01f && t->frame.height == tokens().tagHeight,
           "new: the tag right after the name");
    const Placed* plain = find(rowOf[0], chevron);
    const Placed* marked = find(rowOf[1], chevron);
    expect(plain && marked && marked->frame.x == plain->frame.x, "new: and the row's chevron stays at its end");
}

// Music colours (decisions 052, 053): a page within Appearance with Colours
// from music, a switch, and the colour fade's speed, medium by default.
void coverColoursTests()
{
    PanelHost h;
    std::vector<bool> applied;
    std::vector<FadeSpeed> speeds;
    SettingsProvider settings({ .setCoverColours = [&](bool on) { applied.push_back(on); },
                                .setFadeSpeed = [&](FadeSpeed s) { speeds.push_back(s); } });
    settings.start(h);
    expect(applied == std::vector<bool>{ false } && speeds == std::vector<FadeSpeed>{ FadeSpeed::Medium },
           "music colours: off and medium by default (decision 055), and the app told");
    openWindow(settings);
    expect(detailOf(h, "Music colours") == "Off" && !windowRow(h, "Colours from music"), "music colours: one row on Appearance, saying it's off");
    settings.onAction(fromWindow(P::kOpenMusicColours));
    const WindowPage* page = pageOf(h, kAppearancePage);
    expect(page->title == "Music colours" && page->backAction == P::kBack && !itemOf<Switch>(windowRow(h, "Colours from music"))->on,
           "music colours: its page, the switch off");
    expect(page->sections.size() == 2 && page->sections[1].title == "Colour fade" && itemOf<Button>(windowRow(h, "Medium"))->glyph == Glyph::Checked &&
           windowRow(h, "Fast") && windowRow(h, "Slow"),
           "music colours: the fade's three speeds under their heading, medium picked");
    settings.onAction(fromWindow(P::kToggleCoverColours));
    expect(applied.back() && h.store[SettingsProvider::kCoverColoursKey] == "on" && itemOf<Switch>(windowRow(h, "Colours from music"))->on,
           "music colours: a click switches it on, saved, the page stays");
    settings.onAction(fromWindow(std::string(P::kPickFade) + "2"));
    expect(speeds.back() == FadeSpeed::Slow && h.store[SettingsProvider::kFadeKey] == "slow" && itemOf<Button>(windowRow(h, "Slow"))->glyph == Glyph::Checked,
           "music colours: slow picked, saved, ticked");
    settings.onAction(fromWindow(P::kBack));
    expect(detailOf(h, "Music colours") == "On", "music colours: Appearance says on");
    settings.stop();
    settings.start(h);
    expect(applied.back() && speeds.back() == FadeSpeed::Slow, "music colours: both kept over a restart");
    openWindow(settings);
    settings.onAction(fromWindow(P::kToggleCoverColours));
    expect(!applied.back() && h.store[SettingsProvider::kCoverColoursKey] == "off", "music colours: and back off");
    settings.stop();

    PanelHost bare;
    SettingsProvider without({});
    without.start(bare);
    openWindow(without);
    expect(!windowRow(bare, "Music colours"), "music colours: no hook, no row");
    without.stop();
}

} // namespace

void runSettingsNewTests()
{
    tabRowTests();
    coverColoursTests();
    newTagTests();
    tagLayout();
}

} // namespace cup::test
