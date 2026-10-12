// Keyboard navigation and shortcuts (decision 031): where the arrows go on
// the Settings card, its theme page and Home, and the shortcuts (decision 032).

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
    using P = SettingsProvider;
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

    // Shortcuts (decision 032): system-wide, rebindable, saved as text.
    Chord openHome = chordOf(defaultBindings(), Command::OpenHome);
    expect(chordText(openHome) == "Ctrl+Alt+Space", "shortcuts: Open Home is Ctrl+Alt+Space");
    expect(parseChord("Ctrl+Alt+Space") == openHome && parseChord(" ctrl + shift + f5 ") == Chord{ .key = kF1 + 4, .ctrl = true, .shift = true } &&
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
