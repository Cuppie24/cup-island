// Pomodoro's to-do list (decision 015): the pure list, and the whole loop
// (typing into the card's field, paste, checkboxes, clear) through the registry.

#include "core/dialog.h"
#include "core/hit_test.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"

#include <cmath>
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
    expect(!list.apply(todo_action::kClearDone, "") && list.items().size() == 3, "the clear button alone only asks: nothing cleared");
    expect(list.apply(todo_action::kClearDoneConfirmed, "") && list.items().size() == 2, "confirmed: clear solved removes the done ones");
    expect(!list.apply(todo_action::kClearDoneConfirmed, ""), "nothing solved: no change");
    std::uint64_t first = list.items()[0].id;
    expect(list.apply(std::string(todo_action::kDelete) + std::to_string(first), "") && list.items().size() == 1 &&
           list.items()[0].text == "Ship it", "delete one, at once");
    expect(list.apply(todo_action::kClearAllConfirmed, "") && list.items().empty(), "confirmed: clear all");
    TodoList longOne;
    longOne.add(std::string(500, 'x'));
    expect(longOne.items()[0].text.size() == 200, "a to-do is capped to one line's worth");

    // The loop, with a fake clipboard and storage.
    std::string saved = "[x] Old done\n[ ] Old open\n";
    std::string clipboard = "Email Anna\nBook flights\n";
    IslandState state;
    Ms now{ 0 };
    ProviderRegistry registry{ [&](const Event& e, const char*) { state = reduce(state, e, now); }, [&] { return now; } };
    registry.add(std::make_unique<PomodoroProvider>(TodoStorage{ [&] { return saved; }, [&](const std::string& s) { saved = s; } }));
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

    // A to-do ticks from anywhere on its row: clicking its text hits its checkbox.
    const Placed* openText = firstOf(card, [](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == "Old open"; });
    auto onText = hitTest(card, { openText->frame.x + openText->frame.width - 2, openText->frame.y + openText->frame.height / 2 });
    expect(onText && onText->action.actionId.rfind(todo_action::kToggle, 0) == 0, "clicking a to-do's text ticks it");
    // A list item: lit across the whole row under the pointer, its content
    // inset from that background, and padding between items.
    const Placed* openBox = firstOf(card, [&](const Placed& p) { return p.key == openText->key.substr(0, openText->key.rfind('/')) + "/0"; });
    expect(openText->listRow && openBox && openBox->listRow == openText->listRow, "the row's items share its hover background");
    const Rect& lit = *openText->listRow;
    expect(lit.x == field->frame.x && lit.width == field->frame.width, "as wide as the field");
    expect(openBox->frame.x == lit.x + tokens().listItemInset, "the checkbox inset from its edge");
    expect(lit.height == tokens().smallButtonSize + 2 * tokens().listItemPadding &&
           std::abs(openBox->frame.y + openBox->frame.height / 2 - (lit.y + lit.height / 2)) < 0.01f, "padded above and below, content centred");
    expect(onText && hitTest(card, { lit.x + 1, lit.y + lit.height / 2 })->action.actionId == onText->action.actionId,
           "its whole background ticks it");
    expect(!field->listRow, "the field isn't a list item");
    std::string fieldRow = field->key.substr(0, field->key.rfind('/') + 1);
    std::size_t inFieldRow = 0;
    for (const Placed& p : card.elements)
        if (p.key.rfind(fieldRow, 0) == 0) ++inFieldRow;
    expect(inFieldRow == 1, "the field is alone on its row: no paste button (Ctrl+V pastes)");

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

    // Ctrl+V in the field: the app sends the clipboard as the field's action; every line a to-do.
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kAdd, 0, clipboard });
    expect(saved.find("Email Anna") != std::string::npos && saved.find("Book flights") != std::string::npos, "paste adds each line");

    // Five to-dos now: a window of todoVisibleItems, scrolled to the newest, and what's out of it.
    auto shownTexts = [&] {
        Scene sc = layout(state, now);
        std::vector<std::string> texts;
        for (const Placed& p : sc.elements)
            if (auto* t = std::get_if<Text>(&p.item); t && p.key.rfind("expandedCenter/", 0) == 0 && !t->centred) texts.push_back(t->value);
        return texts;
    };
    auto hasText = [&](const std::string& value) {
        Scene sc = layout(state, now);
        return firstOf(sc, [&](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == value; }) != nullptr;
    };
    Scene full = layout(state, now);
    std::size_t boxes = 0;
    for (const Placed& p : full.elements)
        if (auto* b = std::get_if<Button>(&p.item); b && b->actionId.rfind(todo_action::kToggle, 0) == 0) ++boxes;
    expect(boxes == tokens().todoVisibleItems, "a window of todoVisibleItems to-dos");
    expect(shownTexts().back() == "Book flights" && hasText("1 more above"), "adding scrolls to the newest; what's above is counted");
    expect(full.size.y > tokens().expandedSize.y && full.size.y <= tokens().canvas.y, "the card grew to fit, within the canvas");

    // The wheel over the list scrolls it: the rows carry the scroll action.
    const Placed* listRow = firstOf(full, [](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == "Call mum"; });
    expect(listRow && listRow->scrollAction == todo_action::kScroll, "list rows scroll with the wheel");
    expect(field->scrollAction.empty(), "the field doesn't");
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kScroll, -3 });
    expect(shownTexts().front() == "Old done" && hasText("1 more below"), "scrolled up to the top");
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kScroll, -5 });
    expect(shownTexts().front() == "Old done", "and no further");
    expect(saved.find("Call mum") != std::string::npos, "scrolling doesn't touch the list");

    // A delete button per row, revealed on hover, outside the row's tick area; no question.
    Scene top = layout(state, now);
    const Placed* bin = firstOf(top, [](const Placed& p) { auto* b = std::get_if<Button>(&p.item); return b && b->glyph == Glyph::Delete; });
    expect(bin && std::get<Button>(bin->item).revealOnHover, "each to-do has a delete button, shown on hover");
    auto onBin = hitTest(top, { bin->frame.x + bin->frame.width / 2, bin->frame.y + bin->frame.height / 2 });
    expect(onBin && onBin->action.actionId.rfind(todo_action::kDelete, 0) == 0, "the delete button gets its clicks, not the row's tick");
    registry.route(onBin->action);
    expect(saved.find("Old done") == std::string::npos && !state.dialog, "deleted at once, no question");

    // Clear solved / Clear all ask first, in the host's dialog over the card.
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, std::string(todo_action::kToggle) + "2", 0 }); // "Old open" done
    Vec2 cardSize = layout(state, now).size;
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kClearDone, 0 });
    expect(state.dialog && state.dialog->title == "Clear solved tasks?" && state.dialog->message == "1 task will be removed." &&
           state.dialog->providerId == "pomodoro" && state.dialog->destructive, "Clear solved asks");
    expect(saved.find("Old open") != std::string::npos, "nothing cleared yet");
    Scene asking = layout(state, now);
    const Placed* confirm = firstOf(asking, [](const Placed& p) { auto* b = std::get_if<Button>(&p.item); return b && b->actionId == kDialogConfirm; });
    expect(asking.size == cardSize, "the dialog takes the card's place, at the card's size: nothing morphs");
    expect(confirm && std::get<Button>(confirm->item).label == "Clear" && !firstOf(asking, [](const Placed& p) { return std::holds_alternative<TextInput>(p.item); }),
           "Cancel and Clear instead of the card's content");
    auto onConfirm = hitTest(asking, { confirm->frame.x + confirm->frame.width / 2, confirm->frame.y + confirm->frame.height / 2 });
    expect(onConfirm && onConfirm->action.actionId == kDialogConfirm, "the confirm button is the host's (the app answers it)");
    // Confirm: the app sends the dialog's action, then closes it.
    Dialog asked = *state.dialog;
    registry.route({ asked.providerId, asked.activityId, asked.confirmAction, 0 });
    send(DialogClosed{});
    expect(saved.find("Old open") == std::string::npos && !state.dialog, "confirmed: solved ones cleared");
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kClearAll, 0 });
    expect(state.dialog && state.dialog->confirmAction == todo_action::kClearAllConfirmed, "Clear all asks too");
    send(EscapePressed{});
    expect(!state.dialog && !saved.empty(), "Esc (closing the card) dismisses it: nothing cleared");
    send(PillClicked{});
    send(TileClicked{ PomodoroProvider::kPanelId });
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kClearAll, 0 });
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kClearAllConfirmed, 0 });
    send(DialogClosed{});
    expect(saved.empty(), "confirmed: all cleared");
    Scene empty = layout(state, now);
    expect(!firstOf(empty, [](const Placed& p) { auto* b = std::get_if<Button>(&p.item); return b && !b->label.empty(); }),
           "nothing to clear: no clear buttons");

    // Starting keeps the list on the running card.
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kAdd, 0, "Focus task" });
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStart, 0 });
    Scene running = layout(state, now);
    expect(firstOf(running, [](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == "Focus task"; }),
           "the running card shows the list too");

    // The tallest card: running, a full list, what's out of it, the clear row, and a tab strip.
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
