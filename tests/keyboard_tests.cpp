// Keyboard navigation and shortcuts (decision 031): where the arrows go on
// the Settings card, its theme page and Home, and the shortcuts (decision 032).

#include "core/dialog.h"
#include "core/hit_test.h"
#include "core/keyboard.h"
#include "core/shortcuts.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/system/settings_provider.h"

namespace cup::test {
namespace {

// The Settings card open, as the app shows it.
Scene settingsScene(const SettingsView& view)
{
    IslandState s;
    Panel settings = settingsPanel(view);
    settings.providerId = "system.settings";
    s = reduce(s, PanelPublished{ settings }, Ms{ 0 });
    s = reduce(s, HomeOpened{}, Ms{ 0 });
    s = reduce(s, TileClicked{ settings.id }, Ms{ 0 });
    return layout(s, Ms{ 0 });
}

// The action a focusable's point clicks, or "".
std::string clicks(const Scene& scene, const std::optional<Focusable>& f)
{
    if (!f) return "";
    auto hit = hitTest(scene, f->point);
    return hit ? hit->action.actionId : "";
}

} // namespace

void runKeyboardTests()
{
    namespace P = settings_action;
    Tokens saved = tokens();
    Tokens themed = saved;
    themed.themes.clear();
    for (const char* name : { "Midnight", "Grape", "Lagoon", "Ember", "Lime", "Bubblegum", "Sapphire" }) themed.themes.push_back({ name, {} });
    for (const char* name : { "Paper", "Sorbet", "Mint" }) themed.themes.push_back({ .name = name, .light = true });
    setTokens(themed);

    // The Settings list: the first press shows focus on the first row; then rows top to bottom.
    Scene list = settingsScene({ .theme = "Grape", .startsWithWindows = true });
    auto f = moveFocus(list, "", Arrow::Down);
    expect(clicks(list, f) == P::kOpenThemes, "keys: the first press lands on the first row (Theme)");
    f = moveFocus(list, f->id, Arrow::Down);
    expect(clicks(list, f) == P::kToggleAutostart, "keys: down, the next row");
    f = moveFocus(list, f->id, Arrow::Down);
    expect(clicks(list, f) == P::kOpenShortcuts, "keys: down again");
    f = moveFocus(list, f->id, Arrow::Down);
    expect(clicks(list, f) == P::kCopyLogs, "keys: down to the buttons, the first of them");
    f = moveFocus(list, f->id, Arrow::Right);
    expect(clicks(list, f) == P::kCentreIsland, "keys: right, the one beside it");
    expect(!moveFocus(list, f->id, Arrow::Right), "keys: nothing that way, focus stays");
    auto tab = moveFocus(list, f->id, Arrow::Down);
    expect(tab && tab->id == "tab:", "keys: down from the last row: the Home tab");
    f = moveFocus(list, f->id, Arrow::Up);
    expect(clicks(list, f) == P::kOpenShortcuts, "keys: up, back to the row above");
    expect(!moveFocus(list, f->id, Arrow::Left), "keys: a whole-row item has nothing beside it");

    // The theme page: the back arrow above a list longer than its window.
    Scene themes = settingsScene({ .page = SettingsPage::Themes, .theme = "Grape" });
    f = moveFocus(themes, "", Arrow::Down);
    expect(clicks(themes, f) == P::kBack, "themes: the back arrow first");
    f = moveFocus(themes, f->id, Arrow::Down);
    expect(clicks(themes, f) == std::string(P::kPickTheme) + "0" && f->list, "themes: then the first theme, in the list");
    for (int i = 0; i < 8; ++i) f = moveFocus(themes, f->id, Arrow::Down);
    expect(f && f->list && f->id.find("expandedCenter/") != std::string::npos && f->frame.y > f->list->viewport.y + f->list->viewport.height - 1,
           "themes: down past the window reaches rows out of view (the app scrolls them in)");
    f = moveFocus(themes, f->id, Arrow::Down);
    expect(f && f->list, "themes: on past the headings, into the light ones");
    // Down walks the whole list, past its window and the tabs beside it,
    // and only leaves it after the last row (decision 033).
    f = moveFocus(themes, moveFocus(themes, "", Arrow::Down)->id, Arrow::Down);
    std::size_t walked = 1, lastRow = f->list->row;
    bool inOrder = true;
    for (auto next = moveFocus(themes, f->id, Arrow::Down); next && next->list; next = moveFocus(themes, f->id, Arrow::Down)) {
        inOrder = inOrder && next->list->row > lastRow;
        lastRow = next->list->row;
        f = next;
        ++walked;
    }
    expect(inOrder && walked == themed.themes.size(), "themes: down visits every theme in order, none skipped for a tab");
    auto after = moveFocus(themes, f->id, Arrow::Down);
    expect(!after || after->id.rfind("tab:", 0) == 0, "themes: after the last theme, the tabs");
    auto up = moveFocus(themes, f->id, Arrow::Up);
    expect(up && up->list && up->list->row < f->list->row, "themes: up from the last, the one before it");

    // Esc on one of Settings' pages goes back to its list, like the back
    // arrow; on the list itself there's no page to leave (decision 033).
    auto escOn = [](SettingsPage page) {
        IslandState s;
        Panel p = settingsPanel({ .page = page });
        p.providerId = "system.settings";
        s = reduce(s, PanelPublished{ p }, Ms{ 0 });
        s = reduce(s, HomeOpened{}, Ms{ 0 });
        s = reduce(s, TileClicked{ p.id }, Ms{ 0 });
        return backAction(s);
    };
    expect(escOn(SettingsPage::Themes) == P::kBack && escOn(SettingsPage::Shortcuts) == P::kBack && escOn(SettingsPage::Displays) == P::kBack,
           "esc: a Settings page goes back to the list");
    expect(escOn(SettingsPage::List).empty(), "esc: the list has no page to leave (Esc goes back to Home)");

    // The list of keys scrolls (most rows have nothing to click): down from
    // its last stop scrolls it, focus staying, until its end is in view;
    // then down leaves it. Up scrolls it back the same way (decision 033).
    {
        IslandState s;
        Panel p = settingsPanel({ .page = SettingsPage::Shortcuts });
        p.providerId = "system.settings";
        s = reduce(s, PanelPublished{ p }, Ms{ 0 });
        s = reduce(s, HomeOpened{}, Ms{ 0 });
        s = reduce(s, TileClicked{ p.id }, Ms{ 0 });
        Scene keys = layout(s, Ms{ 0 });
        auto back = moveFocus(keys, "", Arrow::Down);
        auto edit = moveFocus(keys, back->id, Arrow::Down);
        expect(clicks(keys, edit) == std::string(P::kEditShortcut) + "0" && edit->list, "keys page: down from the back arrow, Open Home's row, in the list");
        int scrolls = 0;
        while (auto scroll = scrollInstead(keys, edit->id, Arrow::Down)) {
            expect(scroll->delta > 0 && scroll->list == P::kShortcutList, "keys page: down at the list's last stop scrolls it");
            s = reduce(s, *scroll, Ms{ 0 });
            keys = layout(s, Ms{ 0 });
            if (++scrolls > 50) break;
        }
        auto list = findFocusable(keys, edit->id)->list;
        expect(scrolls > 0 && list->offset >= list->max - 0.5f, "keys page: scrolled to the end, focus still on Open Home");
        expect(!inView(*findFocusable(keys, edit->id)), "keys page: Open Home scrolled out of view: no ring");
        auto out = moveFocus(keys, edit->id, Arrow::Down);
        expect(!out || !out->list, "keys page: at the end, down leaves the list");
        expect(!scrollInstead(keys, back->id, Arrow::Up) && scrollInstead(keys, edit->id, Arrow::Up)->delta < 0,
               "keys page: up scrolls it back; outside the list, nothing to scroll");
    }

    // A dialog keeps focus to its buttons (decision 033): the first press
    // shows it on Cancel, right goes to the answer, Tab wraps; the tabs and
    // the card behind it can't be reached.
    {
        IslandState s;
        Panel p{ .id = "asks", .content = { .label = "asks", .expandedCenter = { row({ Button{ Glyph::Stop, "stop" } }) } } };
        s = reduce(s, PanelPublished{ p }, Ms{ 0 });
        s = reduce(s, HomeOpened{}, Ms{ 0 });
        s = reduce(s, TileClicked{ "asks" }, Ms{ 0 });
        Scene card = layout(s, Ms{ 0 });
        expect(card.tabs && findFocusable(card, "tab:"), "dialog: before it, the card's tabs are stops");
        s = reduce(s, DialogShown{ { .activityId = "asks", .title = "Stop?", .confirmLabel = "Stop", .confirmAction = "stop.yes", .destructive = true } }, Ms{ 0 });
        Scene asked = layout(s, Ms{ 0 });
        std::vector<Focusable> stops = focusables(asked);
        auto cancel = moveFocus(asked, "", Arrow::Down);
        expect(stops.size() == 2 && clicks(asked, cancel) == kDialogCancel, "dialog: only its two buttons; the first press lands on Cancel");
        auto confirm = moveFocus(asked, cancel->id, Arrow::Right);
        expect(clicks(asked, confirm) == kDialogConfirm && clicks(asked, nextFocus(asked, confirm->id)) == kDialogCancel,
               "dialog: right to the answer, Tab round to Cancel");
        expect(!moveFocus(asked, cancel->id, Arrow::Down) && !findFocusable(asked, "tab:"), "dialog: nothing below it, not even the tabs");
    }

    // Home: tiles side by side and in rows.
    IslandState home;
    for (const char* id : { "a", "b", "c", "d", "e" }) {
        Panel p{ .id = id, .content = { .label = id, .expandedCenter = { row({ Text{ id } }) }, .tile = { row({ Text{ id } }) } } };
        home = reduce(home, PanelPublished{ p }, Ms{ 0 });
    }
    home = reduce(home, HomeOpened{}, Ms{ 0 });
    Scene grid = layout(home, Ms{ 0 });
    f = moveFocus(grid, "", Arrow::Right);
    expect(f && f->id == "tile:a", "home: the first press lands on the first tile");
    f = moveFocus(grid, f->id, Arrow::Right);
    expect(f && f->id == "tile:b", "home: right, the next tile");
    auto below = moveFocus(grid, "tile:a", Arrow::Down);
    expect(below && below->id == "tile:e", "home: down, the tile under it");
    expect(!moveFocus(grid, "tile:d", Arrow::Right), "home: the end of a row: nothing further right");
    auto toTab = moveFocus(grid, "tile:e", Arrow::Down);
    expect(!toTab || toTab->id.rfind("tab:", 0) == 0, "home: below the tiles, only the tabs");
    expect(tileAt(grid, f->point) && tileAt(grid, f->point)->id == "b", "home: a tile's point is on the tile");

    // Tab goes on in reading order and round; Home and End jump to the ends.
    expect(nextFocus(grid, "")->id == "tile:a" && nextFocus(grid, "tile:d")->id == "tile:e", "tab: the first, then on into the next row");
    auto last = lastFocus(grid);
    expect(last && firstFocus(grid)->id == "tile:a" && nextFocus(grid, last->id)->id == "tile:a", "tab: after the last, round to the first");
    expect(previousFocus(grid, "tile:e")->id == "tile:d" && previousFocus(grid, "tile:a")->id == last->id && previousFocus(grid, "")->id == "tile:a",
           "shift+tab: the one before, round to the last; nothing focused, the first");
    // 1 to 9: the widgets in publish order.
    expect(homeWidget(grid, 2) && homeWidget(grid, 2)->id == "tile:b" && homeWidget(grid, 5)->id == "tile:e", "digits: the nth widget");
    expect(!homeWidget(grid, 6) && !homeWidget(grid, 0), "digits: past the last, nothing");

    // Static widgets (no card, no tap action: Bluetooth) are stops too, so
    // down from a row of cards lands on the row under it, not the one after.
    IslandState mixed;
    for (const char* id : { "a", "b", "c", "d", "e", "f", "g", "h", "i" }) {
        bool card = std::string("abcdi").find(id) != std::string::npos;
        Panel p{ .id = id, .content = { .label = id, .tile = { row({ Text{ id } }) } } };
        if (card) p.content.expandedCenter = { row({ Text{ id } }) };
        mixed = reduce(mixed, PanelPublished{ p }, Ms{ 0 });
    }
    mixed = reduce(mixed, HomeOpened{}, Ms{ 0 });
    Scene rows = layout(mixed, Ms{ 0 });
    auto onStatic = moveFocus(rows, "tile:b", Arrow::Down);
    expect(onStatic && onStatic->id == "tile:f", "home: down onto a static widget, not past it");
    expect(tileAt(rows, onStatic->point) && !tileAt(rows, onStatic->point)->clickable(), "home: ...which a click does nothing with");
    expect(moveFocus(rows, "tile:f", Arrow::Right)->id == "tile:g" && moveFocus(rows, "tile:f", Arrow::Down)->id == "tile:i",
           "home: and on from it, along the row and down");
    expect(onStatic->radius == rows.tileRadius, "home: a tile's ring has the tile's corners");

    // Ctrl+Tab switches tabs: Home, then each activity, then round;
    // Ctrl+Shift+Tab the other way.
    IslandState tabbed = reduce(home, ActivityStarted{ { .id = "music", .content = { .label = "music", .expandedCenter = { row({ Text{ "m" } }) } } } }, Ms{ 0 });
    tabbed = reduce(tabbed, ActivityStarted{ { .id = "timer", .content = { .label = "timer", .expandedCenter = { row({ Text{ "t" } }) } } } }, Ms{ 0 });
    tabbed = reduce(tabbed, HomeOpened{}, Ms{ 0 });
    Scene onHome = layout(tabbed, Ms{ 0 });
    expect(nextTab(onHome) == std::optional<std::string>("music"), "ctrl+tab: from Home, the first activity");
    expect(previousTab(onHome) == std::optional<std::string>("timer"), "ctrl+shift+tab: from Home, round to the last activity");
    tabbed = reduce(tabbed, ActivitySwitched{ "timer" }, Ms{ 0 });
    Scene onLast = layout(tabbed, Ms{ 0 });
    expect(nextTab(onLast) == std::optional<std::string>(""), "ctrl+tab: from the last tab, round to Home");
    expect(previousTab(onLast) == std::optional<std::string>("music"), "ctrl+shift+tab: the one before");
    expect(nextTab(list) == std::optional<std::string>("") && previousTab(list) == std::optional<std::string>(""),
           "ctrl+tab: on a dormant panel's card (no tab shown), Home either way");
    expect(!nextTab(grid) && !previousTab(grid), "ctrl+tab: no tabs, nothing");

    // Shortcuts (decision 032): system-wide, rebindable, saved as text.
    Chord openHome = chordOf(defaultBindings(), Command::OpenHome);
    expect(chordText(openHome) == "Ctrl+X", "shortcuts: Open Home is Ctrl+X");
    expect(parseChord("Ctrl+Alt+Space") == Chord{ .key = kSpace, .ctrl = true, .alt = true } && parseChord("ctrl+x") == openHome && parseChord(" ctrl + shift + f5 ") == Chord{ .key = kF1 + 4, .ctrl = true, .shift = true } &&
           parseChord("Win+K") == Chord{ .key = 'K', .win = true }, "shortcuts: the text form reads back");
    expect(!parseChord("Ctrl+Enter") && !parseChord("Hyper+X") && !parseChord("Ctrl+") && !parseChord(""), "shortcuts: anything else isn't a chord");
    Bindings reloaded = loadBindings(saveBindings({ { Command::OpenHome, { .key = 'H', .ctrl = true, .alt = true } } }));
    expect(chordText(chordOf(reloaded, Command::OpenHome)) == "Ctrl+Alt+H", "shortcuts: saved and loaded");
    expect(loadBindings("home=Nonsense\nother=Ctrl+Alt+X\n") == defaultBindings() && loadBindings("home=K") == defaultBindings(),
           "shortcuts: what can't be read, or bound, stays the default");
    expect(bindingProblem(defaultBindings(), Command::OpenHome, { .key = 'K' }) == "A shortcut needs Ctrl, Alt or Win" &&
           bindingProblem(defaultBindings(), Command::OpenHome, { .key = 'K', .shift = true }) == "A shortcut needs Ctrl, Alt or Win" &&
           bindingProblem(defaultBindings(), Command::OpenHome, { .key = 'K', .win = true }).empty() &&
           bindingProblem(defaultBindings(), Command::OpenHome, openHome).empty(),
           "shortcuts: system-wide needs Ctrl, Alt or Win; its own keys are no conflict");

    setTokens(saved);
}

} // namespace cup::test
