// Settings' Keyboard shortcuts page (decisions 031, 032, 090): what it lists,
// recording new keys, aborting on any conflict, and letting the keys go
// while a fullscreen app hides the island.

#include "expect.h"
#include "panel_host.h"
#include "core/shortcuts.h"
#include "core/tokens.h"
#include "providers/system/settings_provider.h"

namespace cup::test {
namespace {

Action fromWindow(const std::string& actionId) { return { "system.settings", "", actionId, 0, "", kSettingsWindowId }; }

// The shortcuts page's rows, in order (both sections), and the page.
std::vector<Row> keyRows(const PanelHost& h)
{
    std::vector<Row> rows;
    for (const WindowSection& s : pageOf(h, kGeneralPage)->sections)
        for (const Row& r : s.rows) rows.push_back(r);
    return rows;
}

} // namespace

void runShortcutSettingsTests()
{
    Tokens saved = tokens();
    Tokens plain = saved;
    plain.themes.clear(); // no Theme row: the shortcuts row is the first
    setTokens(plain);
    PanelHost sh;
    SettingsProvider settings({});
    settings.start(sh);
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, settings_action::kOpenWindow, 0 });
    auto press = [&](const std::string& actionId) { settings.onAction(fromWindow(actionId)); };
    // The keyboard shortcuts (decision 031): a row on General, then a page listing core's table.
    expect(windowRow(sh, "Keyboard shortcuts"), "settings: a Keyboard shortcuts row on General");
    press(settings_action::kOpenShortcuts);
    const WindowPage* page = pageOf(sh, kGeneralPage);
    std::vector<Row> keys = keyRows(sh);
    expect(page->title == "Keyboard shortcuts" && page->backAction == settings_action::kBack && page->sections.size() == 2 &&
           page->sections[0].title == "Anywhere" && page->sections[1].title == "On the island" &&
           keys.size() == commands().size() + fixedKeys().size(),
           "shortcuts: a page within General, the system-wide ones, then the island's");
    expect(nameOf(keys[0]) == "Open Home" && detailOf(sh, "Open Home") == "Ctrl+Q" && detailOf(sh, "Open the activity") == "Ctrl+Alt+Q" &&
           detailOf(sh, "Search apps and files") == "Double Ctrl" && windowRow(sh, "Move between items"),
           "shortcuts: each one's keys under what they do");
    expect(keys[0].clickAnywhere && !itemOf<Button>(windowRow(sh, "Move between items")), "shortcuts: the editable ones are clicked; the fixed keys only say");
    press(settings_action::kBack);

    settings.stop();

    // Editing a shortcut (decision 032): click its row, press the new keys.
    // Anything that can't be bound aborts: the old keys stay, the note says why.
    std::map<Command, Chord> registered;
    bool othersHave = false, capturing = false;
    SettingsProvider editor({ .bindHotkey = [&](Command c, const Chord& chord) {
                                if (othersHave && chord.key == 'K') return false;
                                registered[c] = chord;
                                return true;
                            },
                            .unbindHotkeys = [&] { registered.clear(); },
                            .captureKeys = [&](bool on) { capturing = on; } });
    std::string& savedKeys = sh.store[SettingsProvider::kShortcutsKey];
    savedKeys = "";
    editor.start(sh);
    expect(chordText(registered[Command::OpenHome]) == "Ctrl+Q", "keys: the default (Ctrl+Q) registered at start");
    editor.onAction({ "system.settings", SettingsProvider::kPanelId, settings_action::kOpenWindow, 0 });
    auto pressKeys = [&](const std::string& actionId) { editor.onAction(fromWindow(actionId)); };
    auto keysOf = [&] { return detailOf(sh, "Open Home"); };
    auto noteOf = [&] { return pageOf(sh, kGeneralPage)->subtitle; }; // under the page's title, always in view
    pressKeys(settings_action::kOpenShortcuts);
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    expect(capturing && keysOf() == "Press keys…" && noteOf() == "Press the new keys, or Esc to cancel", "keys: a click waits for new keys");
    editor.keysPressed(Chord{ .key = 'J', .ctrl = true, .alt = true });
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && chordText(registered[Command::OpenHome]) == "Ctrl+Alt+J" &&
           savedKeys == "home=Ctrl+Alt+J\nactivity=Ctrl+Alt+Q\nsearch=Double Ctrl\n", "keys: new keys registered, shown and saved");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.keysPressed(Chord{ .key = 'J' });
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && noteOf() == "A shortcut needs Ctrl, Alt or Win" && savedKeys == "home=Ctrl+Alt+J\nactivity=Ctrl+Alt+Q\nsearch=Double Ctrl\n",
           "keys: a bare key aborts, saying why");
    othersHave = true;
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.keysPressed(Chord{ .key = 'K', .ctrl = true, .alt = true });
    expect(keysOf() == "Ctrl+Alt+J" && chordText(registered[Command::OpenHome]) == "Ctrl+Alt+J" &&
           noteOf() == "Ctrl+Alt+K is taken by another app", "keys: one another app has aborts, the old keys registered again");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.keysPressed(std::nullopt);
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && noteOf().empty(), "keys: Esc leaves them as they were, no note");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.shortcutPressed(Command::OpenHome);
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && noteOf().empty(), "keys: its own keys (arriving as the hotkey) change nothing");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    pressKeys(settings_action::kBack);
    expect(!capturing, "keys: leaving the page stops waiting");
    editor.stop();
    editor.start(sh);
    expect(chordText(registered[Command::OpenHome]) == "Ctrl+Alt+J", "keys: the saved ones registered after a restart");
    editor.onAction({ "system.settings", SettingsProvider::kPanelId, settings_action::kOpenWindow, 0 });
    pressKeys(settings_action::kOpenShortcuts);

    // A fullscreen app hides the island: the keys are let go, so that app gets them; back after.
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.setHotkeysActive(false);
    expect(registered.empty() && !capturing, "keys: hidden by a fullscreen app, none registered, no recording");
    editor.setHotkeysActive(true);
    expect(chordText(registered[Command::OpenHome]) == "Ctrl+Alt+J", "keys: shown again, registered again");
    editor.stop();
    registered.clear();
    editor.setHotkeysActive(false);
    editor.start(sh);
    expect(registered.empty(), "keys: started under a fullscreen app, none until it's gone");
    editor.setHotkeysActive(true);
    expect(chordText(registered[Command::OpenHome]) == "Ctrl+Alt+J", "keys: then registered");
    editor.stop();
    savedKeys = "";
    setTokens(saved);

}

} // namespace cup::test
