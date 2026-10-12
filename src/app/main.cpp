// Cup Island entry point: wiring only. Owns the IslandState, turns window
// input into core Events, and pushes the resulting Scene to the renderer.
// The loop: input -> reduce() -> layout() -> renderer + interaction layer.
// Tuning: edit assets/tokens.json while it runs; saving applies it live.
//
// The idle pill shows the time; clicking it opens Home (decisions 009, 019):
// the media and Pomodoro widgets, then theme, Bluetooth and Settings (the
// app theme, display, Start with Windows and Copy logs, decisions 022, 030). Press and hold the pill to lift it
// and move it along the top edge (decision 026).
//
// Keys (decision 031): Ctrl+X opens Home from anywhere (system-wide, off
// while a fullscreen app hides the island, changeable in Settings, decision 032). On the island, the arrows move
// between a card's controls and Home's widgets, Enter or Space clicks.
// Demo keys:
//   M  toggle the demo music player (real media shows by itself)
//   T  toggle a second activity (Split). Click the bubble to open it, or drag
//      it onto the pill to make it the main one.
//   C  post a critical alert               D  toggle a simulated accepted drag
//   H  toggle hidden                       Esc closes what's open (never the app)
// Quit any time from the tray icon's right-click menu (Exit). Everything is logged to
// %LOCALAPPDATA%\CupIsland\logs (platform/log.h); Debug builds also show it in a console.

#include "app/keyboard_input.h"
#include "app/live_tokens.h"
#include "core/dialog.h"
#include "core/hit_test.h"
#include "core/island.h"
#include "core/layout.h"
#include "core/placement.h"
#include "core/tabs.h"
#include "core/tokens.h"
#include "platform/file_watcher.h"
#include "platform/hotkeys.h"
#include "platform/wall_clock.h"
#include "platform/fullscreen_watcher.h"
#include "platform/log.h"
#include "platform/interaction_layer.h"
#include "platform/pill_renderer.h"
#include "platform/autostart.h"
#include "platform/displays.h"
#include "platform/render_layer.h"
#include "platform/single_instance.h"
#include "platform/stay_on_top.h"
#include "platform/text_input.h"
#include "platform/tray_icon.h"
#include "platform/crash_report.h"
#include "platform/watchdog.h"
#include "providers/demo/demo_media_provider.h"
#include "providers/file_store.h"
#include "providers/media/smtc_media_provider.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/system/bluetooth_provider.h"
#include "providers/system/settings_provider.h"
#include "providers/system/theme_provider.h"
#include "providers/provider_registry.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

using namespace cup;
using namespace cup::platform;
// Something in the Windows SDK headers declares a global Chord, so with these using-directives an
// unqualified Chord is ambiguous (MSVC C2872): core types that clash are written cup:: here.

namespace {

// The demo keys (M, T, C, D, H) fake states without a real source: a
// player, a second activity, an alert, a drag. Debug builds only (decision
// 033): in Release a stray key on the island shouldn't do anything. NDEBUG
// is the one build switch C++ already has; CMake sets it for Release.
#ifdef NDEBUG
constexpr bool kDemoKeys = false;
#else
constexpr bool kDemoKeys = true;
#endif

const auto g_startTime = std::chrono::steady_clock::now();
IslandState g_state;
Scene g_scene;
PillRenderer g_renderer;
HWND g_inputHwnd{};
std::filesystem::path g_tokensFile;
RenderLayer g_render;
std::string g_displayId; // the monitor the island is on (decision 023); "" = the main display
std::filesystem::path g_spotsFile;     // where the island rests on each monitor (decision 026)
std::map<std::string, Spot> g_spots;   // ...as last saved, by display id
std::unique_ptr<ProviderRegistry> g_providers;
SettingsProvider* g_settings = nullptr; // owned by g_providers; told when tokens.json reloads

// The pointer over the pill. Feedback is the renderer's; what a click means is
// core's (hitTest). Content reacts to hover, the pill never does (decision 002).
struct Pointer {
    std::optional<Vec2> at;      // over the pill, pill coordinates
    std::string pressedKey;      // the button under the mouse-down
    std::optional<Placed> scrub; // the progress bar being dragged
    float scrubFraction = 0;
    std::optional<Vec2> bubblePress; // mouse-down on the Split bubble, render-layer coordinates
    bool dragged = false;            // ...and core took it as a drag (BubbleDragged): not a click
    std::optional<std::string> tabPress;  // mouse-down on an Expanded tab: its activity ("" = Home)
    std::optional<std::string> tilePress; // mouse-down on a Home tile, off its buttons: its panel
    std::optional<Vec2> islandPress; // mouse-down on the pill that may lift it (decision 026): where it went down
    std::optional<Placed> listPress; // mouse-down on a list row that can be dragged (decision 029)
    float listPressY = 0;            // ...where, pill coordinates: dragged dragStartDistance, it's picked up
} g_pointer;

// Pointer callbacks get render-layer points; content works in pill coordinates.
Vec2 toPill(Vec2 point) { return { point.x - g_scene.offset.x, point.y - g_scene.offset.y }; }
bool onPill(Vec2 point) { return Rect{ g_scene.offset.x, g_scene.offset.y, g_scene.size.x, g_scene.size.y }.contains(point); }
bool onBubble(Vec2 point) { return g_scene.bubble && g_scene.bubble->frame.contains(point); }

Ms now()
{
    return std::chrono::duration_cast<Ms>(std::chrono::steady_clock::now() - g_startTime);
}

// A small text file in %LOCALAPPDATA%\CupIsland, next to the logs folder:
// the island's place on each monitor (decision 026). "" if unknown.
std::filesystem::path dataFile(const wchar_t* name)
{
    return logFolder().empty() ? std::filesystem::path() : logFolder().parent_path() / name;
}

// What providers keep between runs (Host::load/save, decision 028), in
// %LOCALAPPDATA%\CupIsland\store. Files from before it move in on first use.
const FileStore& store()
{
    static const FileStore files = [] {
        FileStore s(dataFile(L"store"));
        s.adopt(dataFile(L"todos.txt"), "pomodoro", PomodoroProvider::kTodosKey);
        s.adopt(dataFile(L"theme.txt"), "system.settings", SettingsProvider::kThemeKey);
        s.adopt(dataFile(L"display.txt"), "system.settings", SettingsProvider::kDisplayKey);
        return s;
    }();
    return files;
}

// Wall time, not now(): what a provider saves has to mean the same moment after a restart.
Ms wallClock() { return std::chrono::duration_cast<Ms>(std::chrono::system_clock::now().time_since_epoch()); }

std::string readTextFile(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

void writeTextFile(const std::filesystem::path& file, const std::string& text)
{
    if (file.empty()) return;
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
}

bool hasActivity(const std::string& id)
{
    for (const auto& a : g_state.activities)
        if (a.id == id) return true;
    return false;
}

const Placed* findElement(const std::string& key)
{
    for (const auto& p : g_scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

// The tab of what's shown (already marked): clicking it does nothing.
bool isCurrent(const Tab& tab)
{
    const auto& strip = *g_scene.tabs;
    return strip.active && &tab == &strip.tabs[*strip.active];
}

// Re-run on every pointer move and every new scene: content can move under a still pointer.
// "<slot>/<row>" of whatever content is under a pill-coordinate point, "" if none.
const Placed* contentUnder(Vec2 point)
{
    for (const Placed& p : g_scene.elements)
        if (p.hitFrame.contains(point) && p.key.rfind("tab/", 0) != 0) return &p;
    return nullptr;
}

std::string rowUnder(Vec2 point)
{
    const Placed* p = contentUnder(point);
    return p ? p->key.substr(0, p->key.rfind('/')) : std::string();
}

// Where hover feedback comes from: the keyboard's focus while it leads (decision 031), else the pointer.
std::optional<Vec2> hoverPoint()
{
    if (!g_pointer.scrub)
        if (auto focused = app::keyFocus()) return focused->point;
    return g_pointer.at;
}

void updateHover()
{
    std::optional<Vec2> at = hoverPoint();
    std::string key;
    if (g_pointer.scrub) {
        key = g_pointer.scrub->key; // stays lit while dragged, even off the bar
    } else if (at) {
        if (auto hit = hitTest(g_scene, *at)) key = hit->key;
    }
    g_renderer.setHovered(key);
    g_renderer.setHoveredRow(at ? rowUnder(*at) : std::string()); // reveals a to-do's delete button

    // Tabs light up as a whole, except the active one, which is already marked (decision 007).
    // Home's tiles too, unless the pointer is on one of their buttons (decision 009).
    const Tab* tab = g_scene.tabs && at ? tabAt(*g_scene.tabs, *at) : nullptr;
    if (tab && isCurrent(*tab)) tab = nullptr;
    const Tile* tile = at && key.empty() ? tileAt(g_scene, *at) : nullptr;
    if (tile && !tile->clickable()) tile = nullptr; // widgets without a card or tap action don't react
    // A list item (a to-do) lights up as a whole row, like a tab (Row::listItem).
    const Placed* item = at && !g_pointer.scrub ? contentUnder(*at) : nullptr;
    if (tab) g_renderer.setAreaHovered(tab->frame, g_scene.tabs->radius);
    else if (tile) g_renderer.setAreaHovered(tile->frame, g_scene.tileRadius);
    else if (item && item->listRow) g_renderer.setAreaHovered(item->listRow, item->listRowRadius);
    else g_renderer.setAreaHovered(std::nullopt, 0);

    // The keyboard's focus also gets a ring (decision 033): hover alone can't
    // tell it from the pointer's, and a static widget has no hover at all.
    std::optional<cup::Focusable> focused;
    if (!g_pointer.scrub) focused = app::keyFocus();
    if (focused) g_renderer.setFocusRing(focused->frame, focused->radius);
    else g_renderer.setFocusRing(std::nullopt, 0);
}

// The Expanded tab under a pill-coordinate point, if any.
const Tab* tabUnder(Vec2 point) { return g_scene.tabs ? tabAt(*g_scene.tabs, point) : nullptr; }

std::optional<Ms> earliest(std::optional<Ms> a, std::optional<Ms> b)
{
    if (!a) return b;
    if (!b) return a;
    return std::min(*a, *b);
}

void saveSpot();
void onLifted();

void dispatch(const Event& event, const char* reason)
{
    Ms t = now();
    Spot spot = g_state.spot;
    bool wasHidden = g_state.hidden;
    std::optional<ShownCard> wasCard = shownCard(g_state);
    g_state = reduce(g_state, event, t);
    Scene next = layout(g_state, t, [](const Text& text) { return g_renderer.measure(text); });

    if (next.mode != g_scene.mode) {
        logLine("%s -> %s (%s) %s", toString(g_scene.mode), toString(next.mode), reason, next.label.c_str());
    }
    g_renderer.apply(next);
    Rect bounds = interactiveBounds(next), was = interactiveBounds(g_scene);
    // Lifted, the window stays where the press went down: it has capture, so
    // every move arrives anyway. Let go, it's over the drop point at once and
    // follows the glide to where the island lands (decision 026).
    if (g_scene.lifted && !next.lifted) jumpTo({ was.width, was.height }, { was.x, was.y });
    if (!next.lifted && (bounds != was || g_scene.lifted)) followMorph({ bounds.width, bounds.height }, { bounds.x, bounds.y });
    bool lifted = next.lifted && !g_scene.lifted;
    g_scene = next;
    if (lifted) onLifted();
    if (g_state.spot != spot) saveSpot();
    updateHover();

    // One timer for everything time-based: core deadlines, ticking labels and provider wake-ups.
    auto deadline = earliest(nextDeadline(g_state), sceneDeadline(g_scene));
    if (g_providers) deadline = earliest(deadline, g_providers->nextWake());
    setTimer(TimerId::Deadline, deadline ? std::optional<Ms>(std::max(*deadline - t, Ms{ 0 })) : std::nullopt);

    // Hidden (a fullscreen app, decision 032): the shortcuts let go of their keys, so that app gets them.
    // Last: Settings republishes, which dispatches again.
    if (g_state.hidden != wasHidden && g_settings) g_settings->setHotkeysActive(!g_state.hidden);
    // A card went: its provider may reset it (Settings opens at its list again).
    if (wasCard && shownCard(g_state) != wasCard && g_providers) g_providers->cardClosed(*wasCard);
}

void toggleProvider(const char* id)
{
    if (g_providers->isRunning(id)) g_providers->stop(id);
    else g_providers->start(id);
}

void toggleTimer()
{
    const std::string id = "demo.timer";
    if (hasActivity(id)) {
        dispatch(ActivityEnded{ id }, "demo key");
        return;
    }
    Content content{ .label = "Timer", .compactLeading = { row({ Icon{ Glyph::Timer } }) },
                     .compactTrailing = { row({ Text{ "4:59", TextStyle::Digits } }) },
                     .minimal = { row({ Icon{ Glyph::Timer } }) },
                     .expandedCenter = { row({ Text{ "Timer", TextStyle::Title } }), row({ Text{ "Demo activity" } }) } };
    dispatch(ActivityStarted{ { .id = id, .content = content } }, "demo key");
}

// A dialog's own buttons (decision 020): Cancel just closes it; the confirm
// button sends the provider its action, then closes it.
void answerDialog(bool confirmed)
{
    if (!g_state.dialog) return;
    Dialog asked = *g_state.dialog;
    dispatch(DialogClosed{}, confirmed ? "dialog confirmed" : "dialog cancelled");
    if (confirmed) g_providers->route({ asked.providerId, asked.activityId, asked.confirmAction, 0 });
}

// A clicked button: the host's dialog answers itself; anything else goes to its provider.
void act(const Action& action)
{
    if (action.actionId == kDialogConfirm || action.actionId == kDialogCancel) { // host-owned names: never a provider's
        answerDialog(action.actionId == kDialogConfirm);
        return;
    }
    g_providers->route(action);
    dispatch(PointerMoved{}, "action"); // the user is busy here: keep Expanded open
}

// The wheel over a list scrolls it, from what's shown (decision 029); the renderer eases it there.
void onWheel(Vec2 canvasPoint, float notches)
{
    const Placed* p = listAt(g_scene, toPill(canvasPoint));
    if (!p) return;
    dispatch(ListScrolled{ p->activityId, p->list->id, p->list->offset, notches * tokens().scrollStep, p->list->max }, "scroll");
}

// An edit button (Button::editsText): its row's text becomes a field holding it (decision 029).
bool startEditing(const Placed& element)
{
    auto* button = std::get_if<Button>(&element.item);
    if (!button || !button->editsText) return false;
    const Text* text = editedText(g_scene, element.activityId, button->actionId);
    if (!text) return true; // nothing to edit on screen: the click does nothing
    SetFocus(g_inputHwnd);
    dispatch(InputFocused{ element.providerId, element.activityId, button->actionId, text->value, /* editing */ true }, "edit");
    return true;
}

// A list row let go after a drag: the drag ends first (the row glides back),
// then its provider moves it, so the glide carries on to the new place.
void dropListRow(bool commit)
{
    std::optional<Placed> dragged;
    for (const Placed& p : g_scene.elements)
        if (p.list && p.list->dragged) dragged = p;
    dispatch(ListDragEnded{}, commit ? "drop row" : "drag cancelled");
    if (commit && dragged && dragged->list->dropAt != dragged->list->row)
        g_providers->route({ dragged->providerId, dragged->activityId, dragged->list->moveAction, static_cast<float>(dragged->list->dropAt) });
}

// Mouse-down: the bubble may become a drag, a progress bar starts scrubbing,
// a button gets its own press feedback, anything else squashes the pill.
void onPressDown(Vec2 canvasPoint)
{
    if (!onBubble(canvasPoint)) {
        // Held still, the island lifts (decision 026). Core ignores it where
        // it can't (Expanded), and then the press is only what it always was.
        dispatch(IslandPressed{}, "press");
        if (g_state.pressedAt) g_pointer.islandPress = canvasPoint;
    }
    if (onBubble(canvasPoint)) {
        g_pointer.bubblePress = canvasPoint;
        g_pointer.dragged = false;
        g_renderer.pressDown(true);
        return;
    }
    Vec2 point = toPill(canvasPoint);
    g_pointer.at = point;
    if (const Tab* tab = tabUnder(point)) {
        g_pointer.tabPress = tab->id; // switches on release, if still on the same tab
        return;
    }
    auto hit = hitTest(g_scene, point);
    const Placed* element = hit ? findElement(hit->key) : nullptr;
    if (const Tile* tile = element ? nullptr : tileAt(g_scene, point)) {
        // Opens its card on release, if still on the same tile. A widget with no
        // card takes the click too, so it doesn't fall through and close Home.
        g_pointer.tilePress = tile->id;
        return;
    }
    if (element && std::holds_alternative<Progress>(element->item)) {
        g_pointer.scrub = *element;
        g_pointer.scrubFraction = hit->action.value;
        g_renderer.scrubElement(hit->key, g_pointer.scrubFraction);
    } else if (element) {
        g_pointer.pressedKey = hit->key;
        g_renderer.pressElement(hit->key);
        // A row that can be reordered: dragged far enough, it's picked up instead of clicked.
        if (element->list && !element->list->moveAction.empty() && !std::holds_alternative<TextInput>(element->item)) {
            g_pointer.listPress = *element;
            g_pointer.listPressY = point.y;
        }
    } else {
        g_renderer.pressDown(false);
    }
    updateHover();
}

void onPointerMoved(Vec2 canvasPoint, bool inside)
{
    app::pointerMoved(canvasPoint); // a real move takes the lead back from the keyboard
    if (g_pointer.islandPress) {
        Vec2 start = *g_pointer.islandPress;
        dispatch(IslandDragged{ { canvasPoint.x - start.x, canvasPoint.y - start.y } }, "lift drag");
        if (g_state.lift) return; // carried: nothing under it reacts
    }
    if (g_pointer.bubblePress) {
        Vec2 start = *g_pointer.bubblePress;
        dispatch(BubbleDragged{ { canvasPoint.x - start.x, canvasPoint.y - start.y } }, "drag");
        if (g_state.bubbleDrag && !g_pointer.dragged) {
            g_pointer.dragged = true;
            g_renderer.pressRelease(true); // un-squash: it's being carried now
        }
        return;
    }
    Vec2 point = toPill(canvasPoint);
    g_pointer.at = inside && onPill(canvasPoint) ? std::optional<Vec2>(point) : std::nullopt;
    if (g_pointer.listPress) {
        const Placed& row = *g_pointer.listPress;
        if (!g_state.listDrag && std::abs(point.y - g_pointer.listPressY) >= tokens().dragStartDistance) {
            if (!g_pointer.pressedKey.empty()) g_renderer.releaseElement(std::exchange(g_pointer.pressedKey, std::string{})); // not a click now
            dispatch(ListDragStarted{ row.activityId, row.list->id, row.list->row, g_pointer.listPressY }, "pick up row");
        }
        if (g_state.listDrag) {
            dispatch(ListDragMoved{ point.y }, "carry row");
            return;
        }
    }
    if (g_pointer.scrub) {
        g_pointer.scrubFraction = seekFraction(*g_pointer.scrub, point.x);
        g_renderer.scrubElement(g_pointer.scrub->key, g_pointer.scrubFraction);
    }
    dispatch(PointerMoved{}, "pointer"); // keeps Expanded open while in use; refreshes hover
}

// What a click does, by what it's on. The pointer's release and the keyboard's Enter share them.
void clickTab(const std::string& id) { dispatch(id.empty() ? Event{ HomeOpened{} } : Event{ ActivitySwitched{ id } }, "tab"); }

void clickTile(const Tile& tile)
{
    if (!tile.tapAction.empty()) {
        g_providers->route({ tile.providerId, tile.id, tile.tapAction, 0 }); // a toggle: no card
        dispatch(PointerMoved{}, "tile action");                              // the user is busy here: keep Home open
    } else if (tile.opens) {
        dispatch(TileClicked{ tile.id }, "tile");
    }
}

void clickElement(const Hit& hit)
{
    const Placed* element = findElement(hit.key);
    if (element && std::holds_alternative<TextInput>(element->item)) {
        // A field: focus it for typing (decision 016); nothing is sent until Enter.
        SetFocus(g_inputHwnd);
        dispatch(InputFocused{ hit.action.providerId, hit.action.activityId, hit.action.actionId }, "focus field");
    } else if (!element || !startEditing(*element)) {
        act(hit.action);
    }
}

void onPressRelease(Vec2 canvasPoint, Release how)
{
    if (g_pointer.islandPress) {
        g_pointer.islandPress.reset();
        bool lifted = g_state.lift.has_value();
        dispatch(IslandReleased{}, lifted ? "landed" : "release");
        if (lifted) return; // it was carried, not clicked; lost capture lands it too
    }
    if (g_pointer.tabPress) {
        std::string id = *std::exchange(g_pointer.tabPress, std::nullopt);
        const Tab* tab = how == Release::Inside && onPill(canvasPoint) ? tabUnder(toPill(canvasPoint)) : nullptr;
        if (tab && tab->id == id && !isCurrent(*tab)) clickTab(id);
        return;
    }
    if (g_pointer.tilePress) {
        std::string id = *std::exchange(g_pointer.tilePress, std::nullopt);
        const Tile* tile = how == Release::Inside && onPill(canvasPoint) ? tileAt(g_scene, toPill(canvasPoint)) : nullptr;
        if (tile && tile->id == id) clickTile(*tile);
        return;
    }
    if (g_pointer.bubblePress) {
        g_pointer.bubblePress.reset();
        if (!g_pointer.dragged) g_renderer.pressRelease(true);
        if (g_state.bubbleDrag) dispatch(how == Release::Cancelled ? Event{ BubbleDragCancelled{} } : Event{ BubbleReleased{} }, "drop");
        else if (!g_pointer.dragged && how == Release::Inside && onBubble(canvasPoint)) dispatch(BubbleClicked{}, "bubble click");
        return;
    }
    if (std::exchange(g_pointer.listPress, std::nullopt) && g_state.listDrag) {
        dropListRow(how != Release::Cancelled); // dropped anywhere: it lands where it shows it would
        return;
    }
    Vec2 point = toPill(canvasPoint);
    if (g_pointer.scrub) {
        // A scrub seeks wherever it's let go, even off the pill; only a lost capture cancels it.
        Placed bar = *std::exchange(g_pointer.scrub, std::nullopt);
        bool commit = how != Release::Cancelled;
        g_renderer.endScrub(bar.key, commit ? std::optional<float>(g_pointer.scrubFraction) : std::nullopt, now());
        if (commit) g_providers->route({ bar.providerId, bar.activityId, "seek", g_pointer.scrubFraction });
        dispatch(PointerMoved{}, "seek");
        return;
    }
    if (!g_pointer.pressedKey.empty()) {
        std::string key = std::exchange(g_pointer.pressedKey, std::string{});
        g_renderer.releaseElement(key);
        // Only a release on the same button counts: sliding off cancels, like any button.
        auto hit = how == Release::Inside && onPill(canvasPoint) ? hitTest(g_scene, point) : std::nullopt;
        if (hit && hit->key == key) clickElement(*hit);
        return;
    }
    g_renderer.pressRelease(false);
    if (how == Release::Inside && onPill(canvasPoint) && !hitTest(g_scene, point)) dispatch(PillClicked{}, "click");
}

// Sends the focused field's text to its provider: what was typed (Enter), or
// the clipboard (Ctrl+V: the provider splits lines into items).
void sendDraft(const std::string& text)
{
    const Draft& d = *g_state.draft;
    if (!text.empty()) g_providers->route({ d.providerId, d.activityId, d.actionId, 0, text });
}

// Typed text, for the focused field (decision 016). Enter sends, Backspace
// erases, Ctrl+V pastes; other control characters are ignored.
void onChar(wchar_t c)
{
    static wchar_t highSurrogate = 0; // the first half of a character outside the BMP (emoji)
    if (!g_state.draft) return;
    if (c == L'\r') {
        sendDraft(g_state.draft->text);
        dispatch(InputSubmitted{}, "enter");
    } else if (c == L'\b') {
        dispatch(TextErased{}, "backspace");
    } else if (c == 0x16) { // Ctrl+V
        sendDraft(readClipboardText(g_inputHwnd));
        dispatch(PointerMoved{}, "paste"); // busy: keep the card open
    } else if (c >= 0xD800 && c <= 0xDBFF) {
        highSurrogate = c;
    } else if (c >= 0x20) {
        wchar_t pair[2] = { highSurrogate, c };
        bool low = highSurrogate && c >= 0xDC00 && c <= 0xDFFF;
        dispatch(TextTyped{ toUtf8(low ? std::wstring_view(pair, 2) : std::wstring_view(&c, 1)) }, "typing");
        highSurrogate = 0;
    }
}

void onKey(WPARAM key)
{
    if (app::recordKey(key)) return; // Settings is recording a shortcut's new keys (decision 032)
    // A dialog: Esc cancels it, the card stays (decision 020).
    if (g_state.dialog) {
        if (key == VK_ESCAPE) answerDialog(false);
        return;
    }
    // A focused field gets the keys (onChar): no demo keys while typing. Esc
    // stops typing; a second Esc closes the card.
    if (g_state.draft) {
        if (key == VK_ESCAPE) dispatch(InputCancelled{}, "esc");
        return;
    }
    // Alt combinations (WM_SYSKEYDOWN) are only for recording a shortcut; Windows handles the rest (Alt+F4).
    bool ctrl = GetKeyState(VK_CONTROL) < 0, alt = GetKeyState(VK_MENU) < 0;
    if (alt) return;
    if (app::navigationKey(key)) return; // the arrows, Tab, Home/End, 1 to 9, Enter and Space (decisions 031, 033)
    switch (key) {
    case VK_ESCAPE: {
        // One step back, never out of the app: quitting is the tray's Exit. A
        // card's own page goes back to its list, a card opened from Home back
        // to Home, anything else closes (decision 033).
        if (mode(g_state) != Mode::Expanded) return;
        std::string back = backAction(g_state);
        if (auto card = shownCard(g_state); card && !back.empty()) act({ card->providerId, card->activityId, back });
        else dispatch(EscapePressed{}, "esc");
        return;
    }
    }
    // Ctrl+W: asks the shown activity's provider to close it, which may end
    // it, ask first or ignore it (decision 034). Home and a dormant card have
    // no activity to close.
    if (ctrl && key == 'W') {
        auto card = shownActivity(g_state);
        if (mode(g_state) == Mode::Expanded && card) {
            g_providers->closeRequested(*card);
            dispatch(PointerMoved{}, "close request"); // busy here: keep the card open for an answer
        }
        return;
    }
    if (ctrl || !kDemoKeys) return; // the demo keys are plain letters (Ctrl+M isn't M), in Debug builds only
    switch (key) {
    case 'M': toggleProvider("demo"); break;
    case 'T': toggleTimer(); break;
    case 'C': {
        Content content{ .label = "Battery 5%", .alertLeading = { row({ Icon{ Glyph::Warning } }) },
                         .alertCenter = { row({ Text{ "Battery 5%", TextStyle::Title } }), row({ Text{ "Plug in soon" } }) } };
        dispatch(AlertPosted{ { .level = Level::Critical, .content = content } }, "demo key");
        break;
    }
    case 'D':
        if (g_state.dragging) dispatch(DragLeft{}, "demo key");
        else dispatch(DragEntered{ true }, "demo key");
        break;
    case 'H': dispatch(HiddenChanged{ !g_state.hidden }, "demo key"); break;
    }
}

// New tokens are active (a tokens.json save, or a new display scale): resize
// the render layer if the canvas changed, rebuild the content, re-layout.
// The render layer and the interaction layer, centred at the top of the island's monitor.
void positionLayers()
{
    resizeRenderLayer(g_render, tokens().canvas, displayFor(g_displayId).bounds);
    setRenderOrigin(g_render.left, g_render.top);
    dispatch(ScreenResized{ static_cast<float>(g_render.width) }, "screen");
}

// The lift began (decision 026): what the press started on gives up (a
// button isn't clicked, a scrub doesn't seek) and the pill un-squashes.
void onLifted()
{
    if (!g_pointer.pressedKey.empty()) g_renderer.releaseElement(std::exchange(g_pointer.pressedKey, std::string{}));
    if (g_pointer.scrub) g_renderer.endScrub(std::exchange(g_pointer.scrub, std::nullopt)->key, std::nullopt, now());
    g_renderer.pressRelease(false);
    g_pointer.at.reset();
    updateHover();
}

// Where the island rests on this monitor, kept for the next start (decision 026).
void saveSpot()
{
    auto it = g_spots.find(g_displayId);
    if (it != g_spots.end() && it->second == g_state.spot) return;
    g_spots[g_displayId] = g_state.spot;
    writeTextFile(g_spotsFile, formatSpots(g_spots));
}

Spot savedSpot(const std::string& displayId)
{
    auto it = g_spots.find(displayId);
    return it == g_spots.end() ? Spot{} : it->second;
}

void tokensChanged(Vec2 oldCanvas, const char* reason)
{
    if (tokens().canvas != oldCanvas) positionLayers();
    g_renderer.refreshAnimations(); // also drops content visuals, so they're redrawn at the new sizes
    dispatch(Tick{}, reason);
}

// Display scale changed (Settings, or the primary display was swapped): every
// length is rescaled (decision 017).
void onDpiChanged(float scale)
{
    Vec2 oldCanvas = tokens().canvas;
    if (app::rescaleTokens(g_tokensFile, scale)) tokensChanged(oldCanvas, "display scale");
}

void onTimer(TimerId id)
{
    if (id == TimerId::Deadline) {
        g_providers->wakeDue(now());
        // Re-read the wall clock every time: it also does Tick's work, and a
        // clock or time-zone change shows within a minute (decision 009).
        dispatch(readWallClock(), "timer");
    } else if (id == TimerId::TokensReload) {
        Vec2 oldCanvas = tokens().canvas;
        if (!app::loadTokens(g_tokensFile, /* startup */ false)) return;
        tokensChanged(oldCanvas, "tokens.json");
        g_settings->refresh(); // the themes may have changed
    }
}

// Put the island on this monitor ("" = the main display, also the fallback
// for one that isn't connected), at that monitor's scaling (decisions 017, 023).
// Also re-run on any display change: the same monitor may have moved or resized.
void placeIsland(const std::string& displayId)
{
    g_displayId = displayId;
    Display display = displayFor(displayId);
    Vec2 oldCanvas = tokens().canvas;
    bool rescaled = app::rescaleTokens(g_tokensFile, display.scale);
    dispatch(SpotRestored{ savedSpot(displayId) }, "display"); // each monitor keeps its own (decision 026)
    positionLayers();
    if (rescaled) tokensChanged(oldCanvas, "display");
    logLine("island on %s (%.0f%%)", display.name.c_str(), display.scale * 100);
}

// The Settings card picked a theme: recolour everything (decision 022).
void applyTheme(const std::string& name)
{
    Vec2 oldCanvas = tokens().canvas;
    if (app::setTheme(name)) tokensChanged(oldCanvas, "theme");
}

} // namespace

int main()
{
    // First, before the log: a turned-away second copy must not touch the
    // running island's log (startLog() rotates it).
    if (!becomeTheOnlyInstance()) return 0;
    startLog();
    watchForCrashes();
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    logLine("Cup Island starting: %ls (pid %lu)", exePath, GetCurrentProcessId());
    reportPreviousCrash(); // a crash too abrupt for watchForCrashes() still shows up here

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    auto queueController = createDispatcherQueueForCurrentThread();

    // Start on the saved monitor straight away (decision 023), at its scaling:
    // tokens.json is in logical pixels, every length is multiplied by it.
    g_displayId = store().load("system.settings", SettingsProvider::kDisplayKey); // the Settings provider's pick
    g_displayId.erase(g_displayId.find_last_not_of(" \t\r\n") + 1);
    Display display = displayFor(g_displayId);
    app::setDisplayScale(display.scale);
    g_tokensFile = app::findTokensFile();
    app::loadTokens(g_tokensFile, /* startup */ true);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    g_render = createRenderLayer(instance, tokens().canvas, display.bounds);
    RenderLayer& render = g_render;
    if (!render.hwnd) {
        logLine("failed to create the render window (0x%08lx)", GetLastError());
        return 1;
    }

    // Where the island rested on this monitor last time (decision 026).
    g_spotsFile = dataFile(L"position.txt");
    g_spots = parseSpots(readTextFile(g_spotsFile));
    g_state = reduce(g_state, ScreenResized{ static_cast<float>(render.width) }, now());
    g_state = reduce(g_state, SpotRestored{ savedSpot(g_displayId) }, now());
    g_scene = layout(g_state, now());
    g_renderer.build(render.compositor, render.root, g_scene);

    InteractionCallbacks callbacks;
    callbacks.onPressDown = onPressDown;
    callbacks.onPressRelease = onPressRelease;
    callbacks.onClickOutside = [] { dispatch(ClickedOutside{}, "click outside"); };
    callbacks.onPointerMoved = onPointerMoved;
    callbacks.onPointerLeft = [] {
        g_pointer.at.reset();
        updateHover();
    };
    callbacks.onKeyDown = onKey;
    callbacks.onHotkey = app::hotkeyPressed;
    callbacks.onChar = onChar;
    callbacks.onDpiChanged = onDpiChanged;
    callbacks.onWheel = onWheel;
    callbacks.onTimer = onTimer;
    callbacks.onDisplaysChanged = [] {
        // Re-lists the monitors and puts the island back where it was picked.
        if (g_settings) g_settings->refreshDisplays();
    };
    callbacks.onShowRequested = [] {
        logLine("launched again: opening Home");
        dispatch(HomeOpened{}, "launched again");
    };

    Rect bounds = interactiveBounds(g_scene);
    g_inputHwnd = createInteractionLayer(instance, render.left, render.top, { bounds.width, bounds.height }, { bounds.x, bounds.y }, callbacks);
    if (!g_inputHwnd) {
        logLine("failed to create the interaction window (0x%08lx)", GetLastError());
        return 1;
    }
    createTrayIcon(instance, [] {
        logLine("exit from the tray");
        DestroyWindow(g_inputHwnd); // ends the message loop
    });
    watchForHangs(g_inputHwnd);

    // WinRT events arrive on background threads; providers hop to this (UI) thread with it.
    auto uiQueue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    auto postToUi = [uiQueue](std::function<void()> work) { uiQueue.TryEnqueue([work = std::move(work)] { work(); }); };

    g_providers = std::make_unique<ProviderRegistry>(dispatch, now, ProviderRegistry::Store{
        .load = [](const std::string& provider, const std::string& key) { return store().load(provider, key); },
        .save = [](const std::string& provider, const std::string& key, const std::string& text) { store().save(provider, key, text); },
        .wallClock = wallClock,
    });
    g_providers->add(std::make_unique<SmtcMediaProvider>(postToUi));
    g_providers->add(std::make_unique<DemoMediaProvider>()); // M key
    g_providers->add(std::make_unique<BluetoothProvider>(postToUi));
    g_providers->add(std::make_unique<ThemeProvider>(postToUi));
    g_providers->add(std::make_unique<PomodoroProvider>());
    auto settings = std::make_unique<SettingsProvider>(SettingsHooks{
        .applyTheme = applyTheme,
        .copyLogs = [] { return copyLogToClipboard(g_inputHwnd); },
        .listDisplays = [] {
            std::vector<DisplayChoice> choices;
            for (const Display& d : listDisplays()) choices.push_back({ d.id, d.name });
            return choices;
        },
        .applyDisplay = placeIsland,
        .centreIsland = [] { dispatch(SpotRestored{}, "centre island"); },
        .startsWithWindows = startsWithWindows,
        .setStartsWithWindows = setStartsWithWindows,
        .bindHotkey = [](cup::Command command, const cup::Chord& chord) { return registerHotkey(g_inputHwnd, command, chord); },
        .unbindHotkeys = [] { unregisterHotkeys(g_inputHwnd); },
        .captureKeys = app::captureKeys,
    });
    g_settings = settings.get();
    g_settings->setHotkeysActive(!g_state.hidden); // started under a fullscreen app: none until it's gone
    app::setKeyboardHooks({
        .state = []() -> const IslandState& { return g_state; },
        .scene = []() -> const Scene& { return g_scene; },
        .dispatch = dispatch,
        .clickTab = clickTab,
        .clickTile = clickTile,
        .clickElement = [](const Hit& hit) {
            g_renderer.pressElement(hit.key); // the same squash a click gives
            g_renderer.releaseElement(hit.key);
            clickElement(hit);
        },
        .window = g_inputHwnd,
        .settings = g_settings,
    });
    g_providers->add(std::move(settings));
    // Home's widgets follow the order panels first appear: media and Pomodoro
    // first (decision 019), then the system tiles.
    g_providers->start("media");
    g_providers->start("pomodoro");
    g_providers->start("system.theme");
    g_providers->start("system.bluetooth");
    g_providers->start("system.settings");
    dispatch(readWallClock(), "clock");

    // TR §9.2: hide over borderless/exclusive fullscreen apps on this monitor.
    watchFullscreen(render.hwnd, [](bool fullscreen) { dispatch(HiddenChanged{ fullscreen }, "fullscreen"); });

    // Debounced: one save fires several notifications; reload once it settles.
    watchDirectoryOf(g_tokensFile, [] { setTimerFromAnyThread(TimerId::TokensReload, Ms{ 50 }); });

    ShowWindow(render.hwnd, SW_SHOWNOACTIVATE);
    ShowWindow(g_inputHwnd, SW_SHOW);
    stayOnTop({ render.hwnd, g_inputHwnd }); // re-raised whenever another app comes to the front

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    int exitCode = static_cast<int>(msg.wParam);
    logLine("exiting");
    markCleanExit();
    // Safety net: if anything below blocks (a provider's WinRT teardown, a
    // static destructor), the process still ends instead of lingering unseen.
    std::thread([] {
        Sleep(3000);
        logLine("exit stuck for 3 s; terminating");
        TerminateProcess(GetCurrentProcess(), 0);
    }).detach();

    removeTrayIcon();
    g_providers.reset(); // stops providers while the window still exists
    DestroyWindow(render.hwnd);

    // The queue finishes shutting down on this thread, so keep pumping until
    // it has. Blocking on it (.get()) deadlocked: Exit left the process running.
    DWORD uiThread = GetCurrentThreadId();
    queueController.ShutdownQueueAsync().Completed([uiThread](auto&&, auto&&) { PostThreadMessageW(uiThread, WM_QUIT, 0, 0); });
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    logLine("exited");
    return exitCode;
}
