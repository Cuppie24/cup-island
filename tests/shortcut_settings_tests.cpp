// Settings' Keyboard shortcuts page (decisions 031, 032): what it lists,
// recording new keys, aborting on any conflict, and letting the keys go
// while a fullscreen app hides the island.

#include "expect.h"
#include "panel_host.h"
#include "core/shortcuts.h"
#include "core/tokens.h"
#include "providers/system/settings_provider.h"

namespace cup::test {

void runShortcutSettingsTests()
{
    Tokens saved = tokens();
    Tokens plain = saved;
    plain.themes.clear(); // no Theme row: the shortcuts row is the first
    setTokens(plain);
    PanelHost sh;
    SettingsProvider settings({});
    settings.start(sh);
    auto press = [&](const std::string& actionId) { settings.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    // The keyboard shortcuts (decision 031): a row, then a page listing core's table.
    expect(cardText(sh.published.back(), 0) == "Keyboard shortcuts", "settings: a Keyboard shortcuts row under the settings");
    press(settings_action::kOpenShortcuts);
    const Slot& keys = sh.published.back().content.expandedCenter;
    expect(cardText(sh.published.back(), 0) == "Keyboard shortcuts" && keys.size() == 3 + commands().size() + fixedKeys().size() &&
           cardText(sh.published.back(), 1) == "Anywhere" && cardItem(sh.published.back(), 2, 0) == "Open Home" &&
           cardItem(sh.published.back(), 2, 1) == "Ctrl+X" && cardItem(sh.published.back(), 4, 0) == "Move between items",
           "shortcuts: a page with each one's keys and what they do");
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
    expect(chordText(registered[Command::OpenHome]) == "Ctrl+X", "keys: the default (Ctrl+X) registered at start");
    auto pressKeys = [&](const std::string& actionId) { editor.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    auto keysOf = [&] { return cardItem(sh.published.back(), 2, 1); };
    auto noteOf = [&] { return cardText(sh.published.back(), sh.published.back().content.expandedCenter.size() - 1); };
    pressKeys(settings_action::kOpenShortcuts);
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    expect(capturing && keysOf() == "Press keys…" && noteOf() == "Press the new keys, or Esc to cancel", "keys: a click waits for new keys");
    editor.keysPressed(Chord{ .key = 'J', .ctrl = true, .alt = true });
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && chordText(registered[Command::OpenHome]) == "Ctrl+Alt+J" &&
           savedKeys == "home=Ctrl+Alt+J\n", "keys: new keys registered, shown and saved");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.keysPressed(Chord{ .key = 'J' });
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && noteOf() == "A shortcut needs Ctrl, Alt or Win" && savedKeys == "home=Ctrl+Alt+J\n",
           "keys: a bare key aborts, saying why");
    othersHave = true;
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.keysPressed(Chord{ .key = 'K', .ctrl = true, .alt = true });
    expect(keysOf() == "Ctrl+Alt+J" && chordText(registered[Command::OpenHome]) == "Ctrl+Alt+J" &&
           noteOf() == "Ctrl+Alt+K is taken by another app", "keys: one another app has aborts, the old keys registered again");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.keysPressed(std::nullopt);
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && noteOf() == "Close",
           "keys: Esc leaves them as they were, no note");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.shortcutPressed(Command::OpenHome);
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && noteOf() == "Close", "keys: its own keys (arriving as the hotkey) change nothing");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    pressKeys(settings_action::kBack);
    expect(!capturing, "keys: leaving the page stops waiting");
    editor.stop();
    editor.start(sh);
    expect(chordText(registered[Command::OpenHome]) == "Ctrl+Alt+J", "keys: the saved ones registered after a restart");

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
