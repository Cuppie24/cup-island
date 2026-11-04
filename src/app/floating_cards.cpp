#include "app/floating_cards.h"

#include "core/float_layout.h"
#include "core/float_placement.h"
#include "core/floating.h"
#include "core/motion.h"
#include "core/tokens.h"
#include "platform/displays.h"
#include "platform/float_window.h"
#include "platform/log.h"
#include "platform/pill_renderer.h"
#include "platform/stay_on_top.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace cup::app {
namespace {

using platform::FloatWindow;
using platform::PillRenderer;

struct FloatCard {
    Surface surface;
    PillRenderer renderer;
    FloatWindow window;
    std::string providerId;
    std::uint64_t raised = 0; // the FloatingCard::raised last acted on
};

FloatHooks g_hooks;
std::map<std::string, std::unique_ptr<FloatCard>> g_cards; // by card id; stable, the windows' callbacks find them here
std::vector<std::unique_ptr<FloatCard>> g_going;           // sent back, shrinking until their animation ends
Places g_places;
bool g_placesRead = false;

// What the last dispatch wants on screen, copied out of the state: the
// windows are changed later, on their own (applyWanted), never inside the
// dispatch. Raising a card gives it the keys, which tells the island at
// once (WM_ACTIVATE: a click outside), which dispatches again and replaces
// the state mid-loop: that read freed memory and opened cards over and over.
struct Wanted {
    FloatingCard card;
    Scene scene;
    FloatSpec spec;
};
std::vector<Wanted> g_wanted;
bool g_wantShown = true;
bool g_applyQueued = false;
bool g_applying = false;

std::string readFile(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// Every monitor as a Screen. The card is laid out at the island's display
// scale for now (decision 081), so every screen gets that scale.
std::vector<Screen> screens()
{
    std::vector<Screen> out;
    float k = g_hooks.scale();
    for (const platform::Display& d : platform::listDisplays()) out.push_back({ d.id, d.workArea, k });
    return out;
}

// Where it is now, to remember: on the screen holding its middle.
void rememberPlace(const FloatCard& card)
{
    Rect at = card.window.card();
    Vec2 middle{ at.x + at.width / 2, at.y + at.height / 2 };
    auto all = screens();
    if (all.empty()) return;
    const Screen* on = &all.front();
    for (const Screen& s : all)
        if (s.workArea.contains(middle)) on = &s;
    remember(g_places, card.surface.cardId, card.providerId, savedPlace(at, *on));
    if (g_hooks.placesFile.empty()) return;
    std::ofstream out(g_hooks.placesFile, std::ios::binary | std::ios::trunc);
    out << formatPlaces(g_places);
}

// Focus comes from Windows (WM_SETFOCUS / WM_KILLFOCUS) in the middle of
// whatever moved it, often a dispatch: queued, so it's an event of its own.
void focusChanged(const std::string& cardId, bool focused)
{
    auto queue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    std::string id = focused ? cardId : std::string();
    auto send = [id] { g_hooks.dispatch(FloatFocused{ id }, id.empty() ? "card lost the keys" : "card has the keys"); };
    if (!queue || !queue.TryEnqueue(send)) send();
}

platform::FloatCallbacks callbacksFor(const std::string& id)
{
    platform::FloatCallbacks c;
    c.onPressDown = [id](Vec2 p) { if (Surface* s = floatSurface(id)) g_hooks.pressDown(*s, p); };
    c.onPressRelease = [id](Vec2 p, platform::Release how) { if (Surface* s = floatSurface(id)) g_hooks.pressRelease(*s, p, how); };
    c.onPointerMoved = [id](Vec2 p, bool inside) { if (Surface* s = floatSurface(id)) g_hooks.pointerMoved(*s, p, inside); };
    c.onPointerLeft = [id] { if (Surface* s = floatSurface(id)) g_hooks.pointerLeft(*s); };
    c.onWheel = [id](Vec2 p, float notches) { if (Surface* s = floatSurface(id)) g_hooks.wheel(*s, p, notches); };
    c.onKeyDown = [](WPARAM key) { g_hooks.keyDown(key); };      // to the card with the keys: this one
    c.onChar = [](wchar_t ch) { g_hooks.character(ch); };
    c.onFocusChanged = [id](bool focused) { focusChanged(id, focused); };
    c.onCloseRequested = [id] { g_hooks.dispatch(FloatClosed{ id }, "alt+f4 on a card"); };
    c.onMoved = [id] {
        if (auto it = g_cards.find(id); it != g_cards.end()) rememberPlace(*it->second);
    };
    return c;
}

void openCard(const Wanted& want, bool shown)
{
    if (!g_placesRead) {
        g_places = parsePlaces(readFile(g_hooks.placesFile));
        g_placesRead = true;
    }
    const FloatingCard& f = want.card;
    Scene scene = want.scene;
    float k = g_hooks.scale();
    std::vector<Rect> taken;
    for (const auto& [id, card] : g_cards) taken.push_back(card->window.card());
    Rect at = placeFloat(want.spec, { scene.size.x / k, scene.size.y / k }, screens(), g_hooks.islandScreen(), g_hooks.pill(),
                         recall(g_places, f.activityId, f.providerId), taken);
    at.width = scene.size.x; // its own size: it fits its content (resizing isn't in yet)
    at.height = scene.size.y;

    auto made = std::make_unique<FloatCard>();
    if (!made->window.create(g_hooks.instance, g_hooks.compositor, at, pillCornerRadius(scene.size), tokens().floatCloseSize,
                             callbacksFor(f.activityId))) {
        g_hooks.dispatch(FloatClosed{ f.activityId }, "card's windows failed"); // logged; back into the pill, not tried again and again
        return;
    }
    // In the list before anything that can send messages (showing, raising):
    // whatever runs meanwhile finds it.
    FloatCard& card = *made;
    g_cards[f.activityId] = std::move(made);
    scene.offset = { card.window.margin(), card.window.margin() };
    card.renderer.buildFloating(g_hooks.compositor, card.window.root(), scene);
    card.surface = Surface{ .cardId = f.activityId, .scene = scene, .renderer = &card.renderer, .input = card.window.input() };
    card.providerId = f.providerId;
    card.raised = f.raised;
    platform::logLine("card floats: %s", f.activityId.c_str());
    card.window.show(shown);
    card.renderer.appear();
    card.window.raise(true);
    platform::raiseIsland(); // the island stays over its cards
}

void updateCard(FloatCard& card, const Wanted& want, bool shown)
{
    Scene scene = want.scene;
    scene.offset = { card.window.margin(), card.window.margin() };
    if (scene.size != card.surface.scene.size) { // it grows or shrinks from its top-left
        Rect at = card.window.card();
        card.window.setCard({ at.x, at.y, scene.size.x, scene.size.y }, pillCornerRadius(scene.size));
    }
    card.renderer.apply(scene, sceneMotion(card.surface.scene, scene, {}, tokens()));
    card.surface.scene = std::move(scene);
    card.window.show(shown);
    if (want.card.raised != card.raised) {
        card.raised = want.card.raised;
        card.window.raise(true);
        platform::raiseIsland();
    }
}

void sendBack(const std::string& id)
{
    auto it = g_cards.find(id);
    if (it == g_cards.end()) return;
    std::unique_ptr<FloatCard> card = std::move(it->second);
    g_cards.erase(it);
    rememberPlace(*card);
    card->window.hideInput(); // no clicks on a card that's going
    FloatCard* going = card.get();
    g_going.push_back(std::move(card));
    going->renderer.vanish([going] {
        // Not destroyed inside its own animation's callback: next, on its own.
        auto drop = [going] { std::erase_if(g_going, [going](const auto& c) { return c.get() == going; }); };
        auto queue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        if (!queue || !queue.TryEnqueue(drop)) drop();
    });
    platform::logLine("card back in the pill: %s", id.c_str());
}

} // namespace

void setFloatHooks(FloatHooks hooks) { g_hooks = std::move(hooks); }

namespace {

void queueApply();

// The windows to what the last dispatch wanted (g_wanted), from a copy: a
// dispatch while this runs (a raise taking the keys from the island) only
// queues another pass.
void applyWanted()
{
    g_applyQueued = false;
    if (g_applying) return queueApply();
    g_applying = true;
    std::vector<Wanted> wanted = g_wanted;
    bool shown = g_wantShown;
    std::vector<std::string> gone;
    for (const auto& [id, card] : g_cards)
        if (std::none_of(wanted.begin(), wanted.end(), [&](const Wanted& w) { return w.card.activityId == id; })) gone.push_back(id);
    for (const std::string& id : gone) sendBack(id);
    for (const Wanted& want : wanted) {
        if (auto it = g_cards.find(want.card.activityId); it != g_cards.end()) updateCard(*it->second, want, shown);
        else openCard(want, shown);
    }
    g_applying = false;
}

void queueApply()
{
    if (g_applyQueued) return;
    g_applyQueued = true;
    auto queue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    if (!queue || !queue.TryEnqueue(applyWanted)) {
        g_applyQueued = false;
        if (!g_applying) applyWanted(); // no queue (shutting down): now, if that's safe
    }
}

} // namespace

void syncFloats(const IslandState& state, Ms now)
{
    // Copied now, while the state is this dispatch's; applied once it's over.
    g_wanted.clear();
    for (const FloatingCard& f : state.floats) {
        const Content* content = cardContent(state, f.activityId);
        g_wanted.push_back({ f, floatScene(state, f, now, g_hooks.measure), content && content->floating ? *content->floating : FloatSpec{} });
    }
    g_wantShown = floatsShown(state);
    queueApply();
}

Surface* floatSurface(const std::string& cardId)
{
    if (cardId.empty()) return nullptr;
    auto it = g_cards.find(cardId);
    return it == g_cards.end() ? nullptr : &it->second->surface;
}

void forEachFloat(const std::function<void(Surface&)>& visit)
{
    for (auto& [id, card] : g_cards) visit(card->surface);
}

void startMoving(const Surface& surface)
{
    if (auto it = g_cards.find(surface.cardId); it != g_cards.end()) it->second->window.startMove();
}

void setFloatTextCursor(const Surface& surface, bool text)
{
    if (auto it = g_cards.find(surface.cardId); it != g_cards.end()) it->second->window.setTextCursor(text);
}

void closeAllFloats()
{
    g_cards.clear();
    g_going.clear();
}

} // namespace cup::app
