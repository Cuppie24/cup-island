// Pomodoro's to-do list (decision 015): the pure list, and the whole loop
// (typing into the card's field, paste, checkboxes, clear) through the registry.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"

#include <functional>

namespace cup::test {
namespace {

// The first element on the card that matches.
const Placed* firstOf(const Scene& scene, const std::function<bool(const Placed&)>& predicate)
{
    for (const Placed& p : scene.elements)
        if (predicate(p)) return &p;
    return nullptr;
}

} // namespace

void runTodoTests()
{
    // The list itself.
    TodoList list;
    expect(!list.add("  \n\n "), "blank lines add nothing");
    expect(list.add("Write spec\n  Review PR  \r\n\nShip it"), "a paste of several lines");
    expect(list.items().size() == 3 && list.items()[1].text == "Review PR", "every non-blank line, trimmed");
    std::uint64_t review = list.items()[1].id;
    expect(list.apply(std::string(todo_action::kToggle) + std::to_string(review), "") && list.items()[1].done, "toggle by id");
    expect(!list.apply("todo.toggle.nope", "") && !list.apply("todo.toggle.999", ""), "bad or unknown ids are ignored");
    TodoList reloaded = TodoList::load(list.save());
    expect(reloaded.items().size() == 3 && reloaded.items()[1].done && !reloaded.items()[0].done &&
           reloaded.items()[2].text == "Ship it", "save and load round-trip");
    expect(list.apply(todo_action::kClearDone, "") && list.items().size() == 2, "clear solved removes the done ones");
    expect(!list.apply(todo_action::kClearDone, ""), "nothing solved: no change");
    expect(list.apply(todo_action::kClearAll, "") && list.items().empty(), "clear all");
    TodoList longOne;
    longOne.add(std::string(500, 'x'));
    expect(longOne.items()[0].text.size() == 200, "a to-do is capped to one line's worth");

    // The loop, with a fake clipboard and storage.
    std::string saved = "[x] Old done\n[ ] Old open\n";
    std::string clipboard = "Email Anna\nBook flights\n";
    IslandState state;
    Ms now{ 0 };
    ProviderRegistry registry{ [&](const Event& e, const char*) { state = reduce(state, e, now); }, [&] { return now; } };
    registry.add(std::make_unique<PomodoroProvider>(TodoStorage{ [&] { return saved; }, [&](const std::string& s) { saved = s; } },
                                                    [&] { return clipboard; }));
    registry.start("pomodoro");
    auto send = [&](const Event& e) { state = reduce(state, e, now); };
    send(PillClicked{});
    send(TileClicked{ PomodoroProvider::kPanelId });
    Scene card = layout(state, now);
    expect(card.activityId == PomodoroProvider::kPanelId, "the dormant card is open");
    const Placed* field = firstOf(card, [](const Placed& p) { return std::holds_alternative<TextInput>(p.item); });
    expect(field && std::get<TextInput>(field->item).placeholder == "Add a to-do…" && !std::get<TextInput>(field->item).focused,
           "the dormant card has the field");
    expect(firstOf(card, [](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == "Old open"; }),
           "the saved list is loaded");
    const Placed* oldDone = firstOf(card, [](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == "Old done"; });
    expect(oldDone && std::get<Text>(oldDone->item).struck, "solved ones are struck through");

    // Click the field (the app sees a TextInput hit and focuses it), type, Enter.
    auto hit = hitTest(card, { field->frame.x + 5, field->frame.y + field->frame.height / 2 });
    expect(hit && hit->action.actionId == todo_action::kAdd, "the field is a hit");
    send(InputFocused{ hit->action.providerId, hit->action.activityId, hit->action.actionId });
    send(TextTyped{ "Call mum" });
    Scene typing = layout(state, now);
    const Placed* live = firstOf(typing, [](const Placed& p) { return std::holds_alternative<TextInput>(p.item); });
    expect(live && std::get<TextInput>(live->item).focused && std::get<TextInput>(live->item).value == "Call mum",
           "the field shows what's typed");
    // Enter: the app routes the draft as the field's action, then empties the field.
    registry.route({ state.draft->providerId, state.draft->activityId, state.draft->actionId, 0, state.draft->text });
    send(InputSubmitted{});
    expect(saved.find("[ ] Call mum") != std::string::npos, "added and saved");
    expect(state.draft && state.draft->text.empty(), "the field stays focused for the next one");

    // Paste button: every clipboard line a to-do.
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kPaste, 0 });
    expect(saved.find("Email Anna") != std::string::npos && saved.find("Book flights") != std::string::npos, "paste adds each line");

    // Five to-dos now: the card shows todoVisibleItems of them, and counts the rest.
    Scene full = layout(state, now);
    std::size_t boxes = 0;
    for (const Placed& p : full.elements)
        if (auto* b = std::get_if<Button>(&p.item); b && b->actionId.rfind(todo_action::kToggle, 0) == 0) ++boxes;
    expect(boxes == tokens().todoVisibleItems, "only the first few to-dos show");
    expect(firstOf(full, [](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == "+1 more"; }),
           "the rest are counted");
    expect(full.size.y > tokens().expandedSize.y && full.size.y <= tokens().canvas.y, "the card grew to fit, within the canvas");

    // Clear solved, then clear all; then the clear buttons go.
    const Placed* clearSolved = firstOf(full, [](const Placed& p) { auto* b = std::get_if<Button>(&p.item); return b && b->label == "Clear solved"; });
    expect(clearSolved && std::get<Button>(clearSolved->item).enabled, "Clear solved is on: one is done");
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kClearDone, 0 });
    expect(saved.find("Old done") == std::string::npos && saved.find("Old open") != std::string::npos, "clear solved");
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kClearAll, 0 });
    expect(saved.empty(), "clear all");
    Scene empty = layout(state, now);
    expect(!firstOf(empty, [](const Placed& p) { auto* b = std::get_if<Button>(&p.item); return b && !b->label.empty(); }),
           "nothing to clear: no clear buttons");

    // Starting keeps the list on the running card.
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kAdd, 0, "Focus task" });
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStart, 0 });
    Scene running = layout(state, now);
    expect(firstOf(running, [](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == "Focus task"; }),
           "the running card shows the list too");

    // The tallest card: running, a full list, "+n more", the clear row, and a tab strip.
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kAdd, 0, "a\nb\nc\nd\ne\nf" });
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, std::string(todo_action::kToggle) + "1", 0 });
    send(ActivityStarted{ { .id = "media", .content = { .label = "Song" } } });
    send(ActivitySwitched{ PomodoroProvider::kPanelId });
    Scene tallest = layout(state, now);
    expect(tallest.tabs && tallest.size.y <= tokens().canvas.y, "the tallest card fits the canvas");
    float strip = tallest.tabs->tabs[0].frame.y;
    for (const Placed& p : tallest.elements)
        if (p.key.rfind("tab/", 0) != 0) expect(p.frame.y + p.frame.height <= strip - tokens().slotGap + 0.01f, "above the strip: " + p.key);
}

} // namespace cup::test
