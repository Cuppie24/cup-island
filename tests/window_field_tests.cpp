// Typing in a provider's window (decision 105): a field gets the keys, keeps
// its text while it hasn't them, sends on Enter; a text edited in place
// sends its new text when done; a live field reports each edit.

#include "core/window_fields.h"
#include "core/window_keys.h"
#include "expect.h"

namespace cup::test {
namespace {

// One page: a field, a live field and a text that edits in place.
OpenWindow typingWindow(const std::string& name = "Milk")
{
    WindowContent c{ .title = "Typing" };
    WindowPage page{ .id = "one", .title = "One" };
    page.sections.push_back({ .rows = { row({ TextInput{ .actionId = "add", .placeholder = "Add…", .maxLength = 5 } }),
                                        row({ TextInput{ .actionId = "find", .changeAction = "find.live" } }),
                                        row({ Text{ .value = name, .editAction = "rename" }, Button{ .glyph = Glyph::Delete, .actionId = "delete" } }) } });
    c.pages = { page };
    return { .key = { "p", "w" }, .content = c, .page = "one" };
}

std::string keyOf(const WindowScene& s, const std::string& actionId)
{
    for (const Placed& p : s.page) {
        if (auto* input = std::get_if<TextInput>(&p.item); input && input->actionId == actionId) return p.key;
        if (auto* text = std::get_if<Text>(&p.item); text && text->editAction == actionId) return p.key;
        if (auto* button = std::get_if<Button>(&p.item); button && button->actionId == actionId) return p.key;
    }
    return {};
}

const TextInput* inputAt(const WindowScene& s, const std::string& key)
{
    for (const Placed& p : s.page)
        if (p.key == key) return std::get_if<TextInput>(&p.item);
    return nullptr;
}

} // namespace

void runWindowFieldTests()
{
    OpenWindow w = typingWindow();
    Vec2 size{ 900, 700 };
    WindowTyping typing;
    auto scene = [&] { return windowLayout(w, size, 0, Ms{ 0 }, {}, &typing); };
    WindowScene s = scene();
    std::string add = keyOf(s, "add"), live = keyOf(s, "find"), name = keyOf(s, "rename");
    expect(isField(s, add) && isField(s, name) && !isField(s, keyOf(s, "delete")), "fields: a TextInput and a text that edits in place, not a button");

    // A click on the field gives it the keys; typing fills it, as on a card.
    expect(!focusField(typing, s, add, std::nullopt) && typing.draft && typing.draft->activityId == "w", "focused: the draft names the window");
    typeText(typing, s, "Eggs");
    s = scene();
    const TextInput* field = inputAt(s, add);
    expect(field && field->focused && field->value == "Eggs" && field->caret == 4, "the page shows the typing, the caret at its end");
    expect(focusedField(s) && focusedField(s)->key == add, "the focused field, found on the page");
    typeText(typing, s, "Bread");
    expect(typing.draft->text == "EggsB", "past maxLength, only what fits (decision 045)");

    // Enter sends the text with the window's id; the field keeps the keys, empty.
    FieldOutcome enter = fieldKey(typing, s, FieldKey::Enter, false, false);
    expect(enter.send && enter.send->actionId == "add" && enter.send->text == "EggsB" && enter.send->windowId == "w" &&
               enter.send->providerId == "p" && enter.send->activityId.empty() && !enter.letGo,
           "Enter: sent from the window");
    expect(typing.draft && typing.draft->text.empty(), "the field stays focused, empty: type the next one");
    expect(!fieldKey(typing, s, FieldKey::Enter, false, false).send, "an empty Enter sends nothing");

    // Several lines pasted go whole (a to-do each); one line goes in at the caret.
    FieldOutcome pasted = typeText(typing, s, "a\r\nb\r\n");
    expect(pasted.send && pasted.send->text == "a\r\nb\r\n" && typing.draft->text.empty(), "several lines: sent at once");
    typeText(typing, s, "ab\r\n");
    expect(typing.draft->text == "ab", "one line pasted: typed");

    // Editing keys work as in any text box (decision 043).
    fieldKey(typing, s, FieldKey::Left, false, true);
    expect(typing.draft->caret == 1 && typing.draft->anchor == 2, "Shift+Left selects");
    fieldKey(typing, s, FieldKey::Backspace, false, false);
    expect(typing.draft->text == "a", "Backspace erases the selection");

    // Another field takes the keys: the first keeps its text (decision 050).
    focusField(typing, s, live, std::nullopt);
    s = scene();
    expect(inputAt(s, add)->value == "a" && !inputAt(s, add)->focused && inputAt(s, live)->focused, "left with its text, unfocused");
    FieldOutcome change = typeText(typing, s, "x");
    expect(change.send && change.send->actionId == "find.live" && change.send->text == "x" && change.send->windowId == "w",
           "a live field reports each edit (decision 079)");
    expect(!fieldKey(typing, s, FieldKey::Left, false, false).send, "a caret move isn't an edit");
    FieldOutcome esc = fieldKey(typing, s, FieldKey::Escape, false, false);
    expect(esc.letGo && !typing.draft, "Esc: the field lets go");
    focusField(typing, s, add, std::nullopt);
    expect(typing.draft->text == "a" && typing.draft->caret == 1, "back in the first: its text, the caret at its end");

    // A text edited in place starts from its own text; done (Enter or a click away), its new text is sent.
    focusField(typing, s, name, std::size_t{ 2 });
    expect(typing.draft && typing.draft->editing && typing.draft->text == "Milk" && typing.draft->caret == 2, "editing in place, caret where clicked");
    s = scene();
    const TextInput* editing = inputAt(s, name);
    expect(editing && editing->inPlace && editing->focused, "drawn as the text it edits");
    typeText(typing, s, "!");
    std::optional<Action> away = blurField(typing, s);
    expect(away && away->actionId == "rename" && away->text == "Mi!lk" && away->windowId == "w" && !typing.draft, "a click away keeps the edit");
    focusField(typing, s, name, std::nullopt);
    fieldKey(typing, s, FieldKey::Backspace, true, false);
    FieldOutcome done = fieldKey(typing, s, FieldKey::Enter, false, false);
    expect(done.send && done.send->actionId == "rename" && done.letGo && !typing.draft, "Enter ends an edit, sent (the provider decides about blank)");
    focusField(typing, s, name, std::nullopt);
    fieldKey(typing, s, FieldKey::Escape, false, false);
    expect(!typing.draft && typing.kept.count("w/rename") == 0, "Esc drops an edit, and keeps nothing");

    // The field went (another page, the row deleted): it lets go, its text kept.
    focusField(typing, s, add, std::nullopt);
    typeText(typing, s, "b");
    w.content.pages[0].sections[0].rows.erase(w.content.pages[0].sections[0].rows.begin());
    s = scene();
    expect(dropGoneField(typing, s) && !typing.draft && typing.kept["w/add"] == "ab", "a field that went lets go, its text kept");
    expect(!dropGoneField(typing, s), "nothing focused: nothing to drop");

    // The keyboard can land on a field: Enter there gives it the keys (the app's part), the stop is a field.
    auto stop = moveWindowFocus(s, "", Arrow::Down);
    expect(stop && isField(s, stop->id.substr(3)), "a field is a keyboard stop");
}

} // namespace cup::test
