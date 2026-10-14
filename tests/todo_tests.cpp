// Pomodoro's to-do list (decision 015): the pure list, and the whole loop
// (typing into the card's field, paste, checkboxes, clear) through the registry.

#include "core/dialog.h"
#include "core/hit_test.h"
#include "core/layout.h"
#include "core/lists.h"
#include "core/text_edit.h"
#include "core/text_wrap.h"
#include "core/item_metrics.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"

#include <algorithm>
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

// Long to-dos wrap onto more lines and their rows grow (decision 045).
void wrapTests()
{
    const Tokens& t = tokens();
    // Word by word, measured (here estimated: half the font size per character, rounded up per word).
    std::string wrapped = wrapText("one two three four", TextStyle::Body, 70, {});
    expect(wrapped.size() == std::string("one two three four").size() && lineCount(wrapped) > 1, "wrapped at spaces, same length");
    expect(wrapText("short", TextStyle::Body, 500, {}) == "short", "short text: one line");
    expect(lineCount(wrapText(std::string(60, 'x'), TextStyle::Body, 50, {})) == 1, "a word longer than the line keeps its own line");

    TodoList list;
    list.add("Short one");
    list.add("A much longer task that will never fit on a single line of the card, however wide it is made");
    for (int i = 0; i < 6; ++i) list.add("Filler " + std::to_string(i));
    IslandState state;
    state = reduce(state, PanelPublished{ pomodoroPanel(list) }, Ms{ 0 });
    state = reduce(state, PillClicked{}, Ms{ 0 });
    state = reduce(state, TileClicked{ PomodoroProvider::kPanelId }, Ms{ 0 });
    Scene card = layout(state, Ms{ 0 });
    auto named = [&](const Scene& sc, const std::string& start) {
        return firstOf(sc, [&](const Placed& p) { auto* x = std::get_if<Text>(&p.item); return x && x->value.rfind(start, 0) == 0; });
    };
    const Placed* shortText = named(card, "Short");
    const Placed* longText = named(card, "A much");
    const std::string& lines = std::get<Text>(longText->item).wrapped;
    expect(lineCount(lines) >= 2 && lines.size() == std::get<Text>(longText->item).value.size(), "a long to-do wraps");
    expect(longText->frame.height == lineCount(lines) * lineHeight(TextStyle::Body), "its text as tall as its lines");
    expect(longText->listRow->height > shortText->listRow->height &&
           longText->listRow->height == longText->frame.height + 2 * t.listItemPadding, "its row grows to fit");
    expect(lineCount(std::get<Text>(shortText->item).wrapped) == 1, "a short one stays on one line");
    float step = t.smallButtonSize + 2 * t.listItemPadding + t.rowSpacing;
    expect(std::abs(longText->list->viewport.height - (t.listVisibleRows * step - t.rowSpacing)) < 0.01f,
           "the list's window still counts rows of one line");
    const Placed* longBox = firstOf(card, [&](const Placed& p) { return p.key == longText->key.substr(0, longText->key.rfind('/')) + "/0"; });
    expect(std::abs(longBox->frame.y + longBox->frame.height / 2 - (longText->frame.y + longText->frame.height / 2)) < 0.01f,
           "the checkbox stays centred on it");

    // Dragged, a wrapped one passes shorter ones all the way to either end of its group.
    {
        IslandState drag = state;
        std::size_t from = longText->list->row;
        float grab = longText->frame.y + longText->frame.height / 2;
        drag = reduce(drag, ListDragStarted{ PomodoroProvider::kPanelId, todo_action::kList, from, grab }, Ms{ 0 });
        drag = reduce(drag, ListDragMoved{ grab - 1000 }, Ms{ 0 });
        auto carried = [](const Scene& sc) -> const Placed* {
            return firstOf(sc, [](const Placed& p) { return p.list && p.list->dragged; });
        };
        Scene up = layout(drag, Ms{ 0 });
        expect(carried(up) && carried(up)->list->dropAt == 0, "a wrapped to-do dragged up lands above the first");
        drag = reduce(drag, ListDragMoved{ grab + 1000 }, Ms{ 0 });
        Scene down = layout(drag, Ms{ 0 });
        expect(carried(down) && carried(down)->list->dropAt == list.items().size() - 1, "...and dragged down, below the last");
    }

    // Edited, the field wraps the same way and grows as it's typed into.
    const Text& text = std::get<Text>(longText->item);
    state = reduce(state, InputFocused{ "pomodoro", PomodoroProvider::kPanelId, text.editAction, text.value, true, std::nullopt,
                                        text.editMaxLength }, Ms{ 0 });
    Scene editing = layout(state, Ms{ 0 });
    const Placed* field = firstOf(editing, [&](const Placed& p) { return p.key == longText->key; });
    const auto& input = std::get<TextInput>(field->item);
    expect(input.wrapped == lines && field->frame.height == longText->frame.height, "the field breaks where the text did");
    state = reduce(state, TextTyped{ " and then some more words to push it onto yet another line of its own" }, Ms{ 0 });
    Scene longer = layout(state, Ms{ 0 });
    const Placed* grown = firstOf(longer, [&](const Placed& p) { return p.key == longText->key; });
    expect(lineCount(std::get<TextInput>(grown->item).wrapped) > lineCount(lines) && grown->frame.height > field->frame.height,
           "typing more wraps onto more lines, and the field grows");
    state = reduce(state, TextTyped{ std::string(300, 'z') }, Ms{ 0 });
    expect(charCount(state.draft->text) == t.todoMaxLength, "and stops at todoMaxLength characters");
    expect(layout(state, Ms{ 0 }).size.y <= t.canvas.y, "the card still fits");

    // A Centred card is wider than Expanded: it still grows to both sides from the island's centre.
    IslandState wide;
    wide = reduce(wide, ScreenResized{ 1920 }, Ms{ 0 });
    wide = reduce(wide, PanelPublished{ pomodoroPanel(list) }, Ms{ 0 });
    wide = reduce(wide, PillClicked{}, Ms{ 0 });
    Scene home = layout(wide, Ms{ 0 });
    wide = reduce(wide, TileClicked{ PomodoroProvider::kPanelId }, Ms{ 0 });
    Scene pomodoro = layout(wide, Ms{ 0 });
    expect(pomodoro.size.x == home.size.x && home.size.x == tokens().expandedSize.x, "the Pomodoro card as wide as Home: every card is (decision 047)");
    expect(std::abs((pomodoro.offset.x + pomodoro.size.x / 2) - (home.offset.x + home.size.x / 2)) < 0.01f &&
           std::abs(pomodoro.offset.x + pomodoro.size.x / 2 - 960) < 0.01f, "centred where Home was: grown to both sides");
}

// The add field's unsent text survives its card closing (decision 050): shown in the field, unfocused.
void keptTextTests()
{
    IslandState state;
    state = reduce(state, PanelPublished{ pomodoroPanel() }, Ms{ 0 });
    state = reduce(state, PillClicked{}, Ms{ 0 });
    state = reduce(state, TileClicked{ PomodoroProvider::kPanelId }, Ms{ 0 });
    state = reduce(state, InputFocused{ "pomodoro", PomodoroProvider::kPanelId, todo_action::kAdd }, Ms{ 0 });
    state = reduce(state, TextTyped{ "Water plants" }, Ms{ 0 });
    state = reduce(state, EscapePressed{}, Ms{ 0 }); // the card goes back to Home
    state = reduce(state, TileClicked{ PomodoroProvider::kPanelId }, Ms{ 0 });
    Scene card = layout(state, Ms{ 0 });
    const Placed* field = firstOf(card, [](const Placed& p) { return std::holds_alternative<TextInput>(p.item); });
    expect(field && !std::get<TextInput>(field->item).focused && std::get<TextInput>(field->item).value == "Water plants",
           "kept text: the field shows it, unfocused, when the card is back");
}

} // namespace

void runTodoTests()
{
    keptTextTests();
    wrapTests();
    // The list itself.
    TodoList list;
    expect(!list.add("  \n\n "), "blank lines add nothing");
    expect(list.add("Write spec\n  Review PR  \r\n\nShip it"), "a paste of several lines");
    expect(list.items().size() == 3 && list.items()[1].text == "Review PR", "every non-blank line, trimmed");
    std::uint64_t review = list.items()[1].id;
    auto texts = [](const TodoList& l) {
        std::string out;
        for (const Todo& t : l.items()) out += (t.done ? "x:" : "o:") + t.text + " ";
        return out;
    };
    expect(list.apply(std::string(todo_action::kToggle) + std::to_string(review), "") &&
           texts(list) == "o:Write spec o:Ship it x:Review PR ", "toggle by id: a solved one goes below the open ones");
    expect(!list.apply("todo.toggle.nope", "") && !list.apply("todo.toggle.999", ""), "bad or unknown ids are ignored");
    list.add("Later");
    expect(texts(list) == "o:Write spec o:Ship it o:Later x:Review PR ", "a new one ends the open ones, above the solved");
    std::uint64_t ship = list.items()[1].id, later = list.items()[2].id;
    expect(list.move(ship, 0) && texts(list) == "o:Ship it o:Write spec o:Later x:Review PR ", "move: to a new index");
    expect(!list.move(later, 9) && !list.move(review, 0), "but only within its group (open, solved)");
    expect(list.apply(std::string(todo_action::kMove) + std::to_string(later), "", 0) && list.items()[0].text == "Later", "move by action: value = index");
    expect(list.apply(std::string(todo_action::kEdit) + std::to_string(later), "  Sooner \nextra", 0) && list.items()[0].text == "Sooner",
           "edit: the first line, trimmed");
    expect(!list.apply(std::string(todo_action::kEdit) + std::to_string(later), "  ", 0) && list.items()[0].text == "Sooner", "blank: no change");
    list.move(list.items()[2].id, 0);
    list.move(later, 2);
    list.rename(later, "Later"); // back as it was, for what follows
    list.toggle(review);
    expect(texts(list) == "o:Write spec o:Ship it o:Later o:Review PR ", "unticked: back with the open ones, at their end");
    list.toggle(review);
    list.remove(list.items()[2].id);
    TodoList reloaded = TodoList::load(list.save());
    expect(texts(reloaded) == texts(list), "save and load round-trip");
    expect(texts(TodoList::load("[x] Done first\n[ ] Open\n")) == "o:Open x:Done first ", "a list saved ungrouped loads grouped");
    expect(!list.apply(todo_action::kClearDone, "") && list.items().size() == 3, "the clear button alone only asks: nothing cleared");
    expect(list.apply(todo_action::kClearDoneConfirmed, "") && list.items().size() == 2, "confirmed: clear solved removes the done ones");
    expect(!list.apply(todo_action::kClearDoneConfirmed, ""), "nothing solved: no change");
    std::uint64_t first = list.items()[0].id;
    expect(list.apply(std::string(todo_action::kDelete) + std::to_string(first), "") && list.items().size() == 1 &&
           list.items()[0].text == "Ship it", "delete one, at once");
    expect(list.apply(todo_action::kClearAllConfirmed, "") && list.items().empty(), "confirmed: clear all");
    TodoList longOne;
    longOne.add(std::string(500, 'x'));
    expect(longOne.items()[0].text.size() == tokens().todoMaxLength, "a to-do is capped at todoMaxLength characters");
    TodoList accented;
    accented.add(std::string(400, 'x').replace(0, 0, "é")); // é is two bytes: counted as one character, never cut in half
    expect(charCount(accented.items()[0].text) == tokens().todoMaxLength && accented.items()[0].text.rfind("é", 0) == 0,
           "capped by characters, not bytes");
    std::uint64_t only = longOne.items()[0].id;
    longOne.apply(std::string(todo_action::kEdit) + std::to_string(only), std::string(300, 'y'));
    expect(longOne.items()[0].text == std::string(tokens().todoMaxLength, 'y'), "renamed: capped too");

    // The loop, with a fake clipboard and storage.
    std::string saved = "[x] Old done\n[ ] Old open\n";
    std::string clipboard = "Email Anna\nBook flights\n";
    IslandState state;
    Ms now{ 0 };
    // The store (decision 028): the list under Pomodoro's "todos".
    ProviderRegistry::Store store{
        .load = [&](const std::string& provider, const std::string& key) { return provider == "pomodoro" && key == "todos" ? saved : ""; },
        .save = [&](const std::string& provider, const std::string& key, const std::string& text) { if (provider == "pomodoro" && key == "todos") saved = text; },
    };
    ProviderRegistry registry{ [&](const Event& e, const char*) { state = reduce(state, e, now); }, [&] { return now; }, store };
    registry.add(std::make_unique<PomodoroProvider>());
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

    // Only the checkbox ticks a to-do; its text is a click into editing it (decision 043).
    const Placed* openText = firstOf(card, [](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == "Old open"; });
    auto onText = hitTest(card, { openText->frame.x + openText->frame.width - 2, openText->frame.y + openText->frame.height / 2 });
    expect(onText && onText->action.actionId.rfind(todo_action::kEdit, 0) == 0 && onText->key == openText->key,
           "clicking a to-do's text edits it, it doesn't tick it");
    // A list item: lit across the whole row under the pointer, its content
    // inset from that background, and padding between items.
    const Placed* openBox = firstOf(card, [&](const Placed& p) { return p.key == openText->key.substr(0, openText->key.rfind('/')) + "/0"; });
    expect(openText->listRow && openBox && openBox->listRow == openText->listRow, "the row's items share its hover background");
    const Rect& lit = *openText->listRow;
    expect(lit.x == field->frame.x && lit.width == field->frame.width, "as wide as the field");
    expect(openBox->frame.x == lit.x + tokens().listItemInset, "the checkbox inset from its edge");
    expect(lit.height == tokens().smallButtonSize + 2 * tokens().listItemPadding &&
           std::abs(openBox->frame.y + openBox->frame.height / 2 - (lit.y + lit.height / 2)) < 0.01f, "padded above and below, content centred");
    auto onEdge = hitTest(card, { lit.x + 1, lit.y + 1 });
    expect(onEdge && onEdge->key == openBox->key, "the checkbox's click area reaches the row's start, top to bottom");
    expect(openBox->hitFrame.x + openBox->hitFrame.width <= openText->frame.x + 0.01f && openBox->hitFrame.height >= lit.height,
           "...and stops where the text starts");
    // Concentric (decision 006): the checkbox as far from the row's top as from its side, its corners the row's less that gap.
    float side = openBox->frame.x - lit.x, above = openBox->frame.y - lit.y;
    expect(std::abs(side - above) < 0.01f, "the checkbox is inset equally on every side");
    expect(std::abs(openBox->cornerRadius - (openBox->listRowRadius - side)) < 0.01f, "its hover shape nests in the row's");
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

    // The to-dos are one list the host scrolls (decision 029): open ones first,
    // a window of listVisibleRows rows, what's outside it clipped.
    const Tokens& tk = tokens();
    auto boxesOf = [](const Scene& sc) {
        std::vector<const Placed*> out;
        for (const Placed& p : sc.elements)
            if (auto* b = std::get_if<Button>(&p.item); b && b->actionId.rfind(todo_action::kToggle, 0) == 0) out.push_back(&p);
        return out;
    };
    auto textNamed = [](const Scene& sc, const std::string& value) {
        return firstOf(sc, [&](const Placed& p) { auto* t = std::get_if<Text>(&p.item); return t && t->value == value; });
    };
    auto rowTexts = [&](const Scene& sc) {
        std::vector<const Placed*> rows;
        for (const Placed& p : sc.elements)
            if (p.list && std::holds_alternative<Text>(p.item)) rows.push_back(&p);
        std::sort(rows.begin(), rows.end(), [](const Placed* a, const Placed* b) { return a->list->row < b->list->row; });
        std::string out;
        for (const Placed* p : rows) out += std::get<Text>(p->item).value + "|";
        return out;
    };
    Scene full = layout(state, now);
    auto boxes = boxesOf(full);
    float step = tk.smallButtonSize + 2 * tk.listItemPadding + tk.rowSpacing;
    expect(rowTexts(full) == "Old open|Call mum|Email Anna|Book flights|Old done|", "one list, open ones first");
    expect(boxes.size() == 5 && boxes[0]->list && boxes[0]->list->id == todo_action::kList, "every to-do is in it");
    Rect window = boxes[0]->list->viewport;
    expect(std::abs(window.height - (tk.listVisibleRows * step - tk.rowSpacing)) < 0.01f, "a window of listVisibleRows rows");
    expect(full.size.y > tk.expandedSize.y && full.size.y <= tk.canvas.y, "the card grew to fit, within the canvas");
    const Placed* lastBox = boxes[4];
    expect(lastBox->frame.y + lastBox->frame.height > window.y + window.height &&
           lastBox->hitFrame.y + lastBox->hitFrame.height <= window.y + window.height + 0.01f, "the last one runs past it: clipped, clicks too");
    expect(listAt(full, { window.x + 5, window.y + 5 }) && !listAt(full, { field->frame.x + 5, field->frame.y + 5 }), "the wheel works over the list only");

    // The wheel: the app sends how far, from what's shown, up to the max.
    std::string key = listKey(PomodoroProvider::kPanelId, todo_action::kList);
    auto wheel = [&](float delta) {
        Scene shown = layout(state, now);
        const Placed* any = listAt(shown, { window.x + 5, window.y + 5 });
        send(ListScrolled{ any->activityId, any->list->id, any->list->offset, delta, any->list->max });
    };
    wheel(1000);
    Scene bottom = layout(state, now);
    float max = 5 * step - tk.rowSpacing - window.height;
    expect(std::abs(state.scrolls[key].offset - max) < 0.01f && std::abs(boxesOf(bottom)[0]->frame.y - (boxes[0]->frame.y - max)) < 0.01f,
           "scrolled to the end, no further");
    wheel(-1000);
    expect(state.scrolls[key].offset == 0, "and back to the top, no further");
    wheel(1000);

    // A delete button per row, revealed on hover, outside the row's tick area; no question.
    Scene top = layout(state, now);
    const Placed* doneText = textNamed(top, "Old done");
    const Placed* bin = firstOf(top, [&](const Placed& p) {
        auto* b = std::get_if<Button>(&p.item);
        return b && b->glyph == Glyph::Delete && p.list && p.list->row == doneText->list->row;
    });
    expect(bin && std::get<Button>(bin->item).revealOnHover, "each to-do has a delete button, shown on hover");
    auto onBin = hitTest(top, { bin->frame.x + bin->frame.width / 2, bin->frame.y + bin->frame.height / 2 });
    expect(onBin && onBin->action.actionId.rfind(todo_action::kDelete, 0) == 0, "the delete button gets its clicks, not the row's tick");
    registry.route(onBin->action);
    expect(saved.find("Old done") == std::string::npos && !state.dialog, "deleted at once, no question");

    // A click on the task (not its checkbox or delete) edits it in place: no edit button (decision 043).
    Scene editable = layout(state, now);
    const Placed* mum = textNamed(editable, "Call mum");
    expect(!firstOf(editable, [](const Placed& p) { auto* b = std::get_if<Button>(&p.item); return b && b->glyph == Glyph::Edit; }),
           "no edit button");
    expect(mum->frame.x + mum->frame.width >= bin->frame.x - tokens().itemSpacing - tokens().editInset - 0.01f, "the text reaches the delete button");
    auto onTask = hitTest(editable, { bin->frame.x - tokens().itemSpacing, mum->frame.y + mum->frame.height / 2 });
    expect(onTask && onTask->key == mum->key, "past the end of its text, a click still edits it");
    const std::string editId = onTask->action.actionId;
    const Text* edited = editedText(editable, PomodoroProvider::kPanelId, editId);
    expect(edited && edited->value == "Call mum" && editId.rfind(todo_action::kEdit, 0) == 0, "it edits the row's text");
    send(InputFocused{ "pomodoro", PomodoroProvider::kPanelId, editId, edited->value, /* editing */ true });
    send(TextErased{});
    send(TextErased{});
    send(TextErased{});
    send(TextTyped{ "dad" });
    Scene editing = layout(state, now);
    const Placed* inPlace = firstOf(editing, [&](const Placed& p) { return p.key == mum->key; });
    expect(inPlace && std::holds_alternative<TextInput>(inPlace->item) && std::get<TextInput>(inPlace->item).value == "Call dad" &&
           std::get<TextInput>(inPlace->item).focused, "the text is a field holding the edit");
    const Placed* rowBox = firstOf(editing, [&](const Placed& p) { return p.key == mum->key.substr(0, mum->key.rfind('/')) + "/0"; });
    expect(rowBox && inPlace->frame.height == rowBox->frame.height && inPlace->frame.y == rowBox->frame.y &&
           inPlace->cornerRadius == rowBox->cornerRadius, "the field is as tall as the checkbox, its corners nested like it");
    const Placed* rowBin = firstOf(editing, [&](const Placed& p) {
        auto* b = std::get_if<Button>(&p.item);
        return b && b->glyph == Glyph::Delete && p.list && p.list->row == mum->list->row;
    });
    expect(rowBin && std::abs(inPlace->frame.x - (rowBox->frame.x + rowBox->frame.width) - tokens().itemSpacing) < 0.01f &&
           std::abs(rowBin->frame.x - (inPlace->frame.x + inPlace->frame.width) - tokens().itemSpacing) < 0.01f,
           "its background keeps the row's gap to the checkbox and the delete button");
    expect(std::abs(mum->frame.x - (inPlace->frame.x + tokens().editInset)) < 0.01f, "and the text doesn't move when editing starts");
    auto onField = hitTest(editing, { inPlace->frame.x + inPlace->frame.width / 2, inPlace->frame.y + inPlace->frame.height / 2 });
    expect(onField && onField->action.actionId == editId, "clicking it stays in the field: no tick");
    registry.route({ state.draft->providerId, state.draft->activityId, state.draft->actionId, 0, state.draft->text }); // Enter
    send(InputSubmitted{});
    expect(saved.find("[ ] Call dad") != std::string::npos && !state.draft, "Enter renames it, and the edit is over");
    send(InputFocused{ "pomodoro", PomodoroProvider::kPanelId, editId, "Call dad", true });
    send(TextTyped{ " now" });
    send(InputCancelled{});
    expect(saved.find("Call dad now") == std::string::npos && !state.draft, "Esc leaves it as it was");

    // Drag to reorder: the row follows the pointer, the others make room, open ones stay above solved.
    wheel(-1000);
    Scene before = layout(state, now);
    const Placed* openOne = textNamed(before, "Old open");
    float grab = openOne->frame.y;
    send(ListDragStarted{ PomodoroProvider::kPanelId, todo_action::kList, openOne->list->row, grab });
    send(ListDragMoved{ grab + 2.6f * step });
    Scene dragging = layout(state, now);
    const Placed* carried = firstOf(dragging, [&](const Placed& p) { return p.key == openOne->key; });
    const Placed* dad = textNamed(dragging, "Call dad");
    expect(carried && carried->list->dragged && std::abs(carried->frame.y - (grab + 2.6f * step)) < 0.01f, "it follows the pointer");
    expect(carried && carried->list->dropAt == 2 && dad && std::abs(dad->frame.y - openOne->frame.y) < 0.01f, "the ones it passed move up");
    send(ListDragMoved{ grab + 20 * step });
    Scene distant = layout(state, now);
    std::size_t openCount = 4; // Old open, Call dad, Email Anna, Book flights
    const Placed* farCarried = firstOf(distant, [&](const Placed& p) { return p.key == openOne->key; });
    expect(farCarried && farCarried->list->dropAt == openCount - 1, "never below the last open one");
    send(ListDragMoved{ grab + 2.6f * step });
    Scene drop = layout(state, now);
    const Placed* dropped = firstOf(drop, [&](const Placed& p) { return p.key == openOne->key; });
    registry.route({ dropped->providerId, dropped->activityId, dropped->list->moveAction, static_cast<float>(dropped->list->dropAt) }); // let go
    send(ListDragEnded{});
    Scene after = layout(state, now);
    expect(!state.listDrag && rowTexts(after) == "Call dad|Email Anna|Old open|Book flights|", "dropped: the list is reordered");
    expect(textNamed(after, "Old open")->key == openOne->key, "its elements keep their key: the visuals glide to the new place");
    expect(saved.find("[ ] Email Anna\n[ ] Old open\n") != std::string::npos, "and saved in that order");

    // Adding one keeps it in view, even if the list is longer than the window.
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kAdd, 0, "x\ny\nz" });
    Scene revealed = layout(state, now);
    const Placed* z = textNamed(revealed, "z");
    expect(z && z->frame.y >= window.y - 0.01f && z->frame.y + z->frame.height <= window.y + window.height + 0.01f && state.scrolls[key].reveal,
           "the new one is scrolled into view");
    wheel(-40);
    expect(!state.scrolls[key].reveal, "the wheel takes over from there");

    // A solved one can't be carried up among the open ones (sortGroup).
    Scene beforeTick = layout(state, now);
    const Placed* zBox = boxesOf(beforeTick).back(); // "z", the last open one
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, std::get<Button>(zBox->item).actionId, 0 });
    Scene grouped = layout(state, now);
    const Placed* solved = textNamed(grouped, "z");
    expect(solved && std::get<Text>(solved->item).struck && solved->list->row == 6, "ticked: it's the solved group's first");
    send(ListDragStarted{ PomodoroProvider::kPanelId, todo_action::kList, solved->list->row, solved->frame.y });
    send(ListDragMoved{ solved->frame.y - 10 * step });
    Scene carriedUp = layout(state, now);
    const Placed* liftedZ = textNamed(carriedUp, "z");
    expect(liftedZ && liftedZ->list->dropAt == 6, "it stays below the open ones");
    send(ListDragEnded{});
    registry.route({ "pomodoro", PomodoroProvider::kPanelId, std::get<Button>(zBox->item).actionId, 0 }); // open again

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
    float strip = tallest.tabs->tabs[0].frame.y + tokens().tabHeight;
    for (const Placed& p : tallest.elements)
        if (p.key.rfind("tab/", 0) != 0)
            expect(p.frame.y >= strip + tokens().slotGap - 0.01f && p.frame.y + p.frame.height <= tallest.size.y - tokens().expandedPadding + 0.01f,
                   "under the strip, inside the card: " + p.key);
}

} // namespace cup::test
