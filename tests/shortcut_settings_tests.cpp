// Settings' Keyboard shortcuts page (decisions 031, 032): what it lists,
// recording new keys, aborting on any conflict, and letting the keys go
// while a fullscreen app hides the island.

#include "core/layout.h"
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
    expect(rowNamed(sh.published.back(), "Keyboard shortcuts"), "settings: a Keyboard shortcuts row under the settings");
    press(settings_action::kOpenShortcuts);
    const Slot& keys = sh.published.back().content.expandedCenter;
    expect(cardText(sh.published.back(), 0) == "Keyboard shortcuts" && keys.size() == 4 + commands().size() + fixedKeys().size() &&
           cardText(sh.published.back(), 1) == "Anywhere" && cardItem(sh.published.back(), 2, 0) == "Open Home" &&
           cardItem(sh.published.back(), 2, 1) == "Ctrl+Q" && cardItem(sh.published.back(), 3, 0) == "Open the activity" &&
           cardItem(sh.published.back(), 3, 1) == "Ctrl+Alt+Q" && cardItem(sh.published.back(), 4, 0) == "Search apps and files" &&
           cardItem(sh.published.back(), 4, 1) == "Double Ctrl" && cardItem(sh.published.back(), 5, 0) == "Reset all to defaults" &&
           cardItem(sh.published.back(), 7, 0) == "Move between items",
           "shortcuts: a page with each one's keys and what they do");
    expect(rowNamed(sh.published.back(), "Move between items")->grouped && !rowNamed(sh.published.back(), "Move between items")->listItem &&
           rowNamed(sh.published.back(), "Open Home")->listItem && rowNamed(sh.published.back(), "Open Home")->grouped,
           "shortcuts: two groups; the fixed keys' rows are plain, nothing lit (decision 090)");
    // Every key's row one height, editable or not; the page one list as tall as the card allows (decision 092).
    {
        Panel page = sh.published.back();
        page.providerId = "system.settings";
        IslandState st;
        st = reduce(st, PanelPublished{ page }, Ms{ 0 });
        st = reduce(st, HomeOpened{}, Ms{ 0 });
        st = reduce(st, TileClicked{ kSettingsPanelId }, Ms{ 0 });
        Scene scene = layout(st, Ms{ 0 });
        float editable = 0, fixed = 0;
        for (const Placed& e : scene.elements) {
            if (e.key == "expandedCenter/2/group") editable = e.frame.height;            // Open Home: not its group's last
            if (e.key == "expandedCenter/7/group") fixed = e.frame.height;               // Move between items: nor this
        }
        expect(editable > 0 && editable == fixed && std::holds_alternative<Keys>(keys[7].items[1]) &&
               keys[2].visibleRows == tokens().listCardVisibleRows && scene.size.y <= tokens().canvas.y,
               "shortcuts: the fixed keys' rows as tall as the editable ones, keys in caps, the list the card's height");
    }
    // Reset (decision 103): on every command's row and one for them all, dimmed while nothing differs from the defaults.
    {
        const Row* home = rowNamed(sh.published.back(), "Open Home");
        const Row* all = rowNamed(sh.published.back(), "Reset all to defaults");
        const Button& reset = std::get<Button>(home->items[2]);
        expect(reset.glyph == Glyph::Reset && !reset.enabled && std::get<Button>(home->items[3]).glyph == Glyph::Edit &&
                   !std::get<Button>(all->items[1]).enabled,
               "shortcuts: a reset on each row and one for all, dimmed at the defaults");
    }
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
    auto pressKeys = [&](const std::string& actionId) { editor.onAction({ "system.settings", SettingsProvider::kPanelId, actionId, 0 }); };
    auto keysOf = [&] { return cardItem(sh.published.back(), 2, 1); };
    auto noteOf = [&] { return cardText(sh.published.back(), sh.published.back().content.expandedCenter.size() - 1); };
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
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && noteOf() == fixedKeys().back().does,
           "keys: Esc leaves them as they were, no note");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.shortcutPressed(Command::OpenHome);
    expect(!capturing && keysOf() == "Ctrl+Alt+J" && noteOf() == fixedKeys().back().does, "keys: its own keys (arriving as the hotkey) change nothing");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    pressKeys(settings_action::kBack);
    expect(!capturing, "keys: leaving the page stops waiting");
    editor.stop();
    editor.start(sh);
    expect(chordText(registered[Command::OpenHome]) == "Ctrl+Alt+J", "keys: the saved ones registered after a restart");

    // Reset (decision 103): one command back to its default, or all of them.
    auto resetOf = [&](const char* does) { return std::get<Button>(rowNamed(sh.published.back(), does)->items[2]); };
    pressKeys(settings_action::kOpenShortcuts);
    expect(resetOf("Open Home").enabled && !resetOf("Open the activity").enabled &&
               std::get<Button>(rowNamed(sh.published.back(), "Reset all to defaults")->items[1]).enabled,
           "reset: lit on the changed row and for all, dimmed on the default one");
    pressKeys(std::string(settings_action::kResetShortcut) + "0");
    expect(keysOf() == "Ctrl+Q" && chordText(registered[Command::OpenHome]) == "Ctrl+Q" && savedKeys == "home=Ctrl+Q\nactivity=Ctrl+Alt+Q\nsearch=Double Ctrl\n" &&
               !resetOf("Open Home").enabled, "reset: one row back to its default, registered and saved");
    // Swapped keys: reset one at a time, each clashes with the other's; reset all puts both back.
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.keysPressed(Chord{ .key = 'J', .ctrl = true, .alt = true });
    pressKeys(std::string(settings_action::kEditShortcut) + "1");
    editor.keysPressed(Chord{ .key = 'Q', .ctrl = true });
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.keysPressed(Chord{ .key = 'Q', .ctrl = true, .alt = true });
    expect(keysOf() == "Ctrl+Alt+Q" && cardItem(sh.published.back(), 3, 1) == "Ctrl+Q", "reset: (the keys swapped)");
    pressKeys(std::string(settings_action::kResetShortcut) + "0");
    expect(keysOf() == "Ctrl+Alt+Q" && noteOf() == "Ctrl+Q is already Open the activity", "reset: a row whose default another command has aborts, saying why");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    pressKeys(settings_action::kResetShortcuts);
    expect(!capturing && keysOf() == "Ctrl+Q" && cardItem(sh.published.back(), 3, 1) == "Ctrl+Alt+Q" &&
               chordText(registered[Command::OpenHome]) == "Ctrl+Q" && chordText(registered[Command::OpenActivity]) == "Ctrl+Alt+Q" &&
               savedKeys == "home=Ctrl+Q\nactivity=Ctrl+Alt+Q\nsearch=Double Ctrl\n" && noteOf() == fixedKeys().back().does,
           "reset all: every command back to its default, registered and saved, recording stopped");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    editor.keysPressed(Chord{ .key = 'J', .ctrl = true, .alt = true });
    pressKeys(settings_action::kBack);

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
