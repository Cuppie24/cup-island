// Settings' Keyboard shortcuts page (decisions 031, 032, 104): what it lists,
// recording new keys, aborting on any conflict, and letting the keys go
// while a fullscreen app hides the island.

#include "core/layout.h"
#include "expect.h"
#include "panel_host.h"
#include "core/shortcuts.h"
#include "core/tokens.h"
#include "providers/system/settings_provider.h"

namespace cup::test {
namespace {

Action fromWindow(const std::string& actionId) { return { "system.settings", "", actionId, 0, "", kSettingsWindowId }; }

// A key's row's keys: its key caps (decision 092), or the note there while it waits ("Press keys…").
std::string keysIn(const Row* r)
{
    if (auto* keys = itemOf<Keys>(r)) return keys->text;
    for (std::size_t i = 1; r && i < r->items.size(); ++i)
        if (auto* text = std::get_if<Text>(&r->items[i])) return text->value;
    return "?";
}

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
           keys.size() == commands().size() + 1 + fixedKeys().size() && nameOf(keys[commands().size()]) == "Reset all to defaults",
           "shortcuts: a page within General, the system-wide ones and a reset for all, then the island's");
    expect(nameOf(keys[0]) == "Open Home" && keysIn(windowRow(sh, "Open Home")) == "Ctrl+Q" && keysIn(windowRow(sh, "Open the activity")) == "Ctrl+Alt+Q" &&
           keysIn(windowRow(sh, "Search apps and files")) == "Double Ctrl" && itemOf<Keys>(windowRow(sh, "Move between items")),
           "shortcuts: each one's keys as key caps at its end (decision 092)");
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
    auto keysOf = [&] { return keysIn(windowRow(sh, "Open Home")); };
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

    // Reset (decision 103): one command back to its default, or all of them.
    auto resetOf = [&](const char* does) { return std::get<Button>(windowRow(sh, does)->items[2]); };
    expect(resetOf("Open Home").enabled && resetOf("Open Home").glyph == Glyph::Reset && !resetOf("Open the activity").enabled &&
               itemOf<Button>(windowRow(sh, "Reset all to defaults"))->enabled,
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
    expect(keysOf() == "Ctrl+Alt+Q" && keysIn(windowRow(sh, "Open the activity")) == "Ctrl+Q", "reset: (the keys swapped)");
    pressKeys(std::string(settings_action::kResetShortcut) + "0");
    expect(keysOf() == "Ctrl+Alt+Q" && noteOf() == "Ctrl+Q is already Open the activity", "reset: a row whose default another command has aborts, saying why");
    pressKeys(std::string(settings_action::kEditShortcut) + "0");
    pressKeys(settings_action::kResetShortcuts);
    expect(!capturing && keysOf() == "Ctrl+Q" && keysIn(windowRow(sh, "Open the activity")) == "Ctrl+Alt+Q" &&
               chordText(registered[Command::OpenHome]) == "Ctrl+Q" && chordText(registered[Command::OpenActivity]) == "Ctrl+Alt+Q" &&
               savedKeys == "home=Ctrl+Q\nactivity=Ctrl+Alt+Q\nsearch=Double Ctrl\n" && noteOf().empty(),
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
