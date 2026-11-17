#include "core/window_fields.h"

#include "core/card_overlays.h"
#include "core/dialog.h"
#include "core/text_edit.h"

#include <sstream>
#include <vector>

namespace cup {
namespace {

// What can be typed in: the page's items, and a question's (its field, decision 142) while one is up.
template <typename Visit>
const Placed* findField(const WindowScene& s, Visit visit)
{
    if (s.dialog)
        for (const Placed& p : s.dialog->elements)
            if (visit(p)) return &p;
    for (const Placed& p : s.page)
        if (visit(p)) return &p;
    return nullptr;
}

const Placed* findOnPage(const WindowScene& s, const std::string& key)
{
    return findField(s, [&](const Placed& p) { return p.key == key; });
}

// The TextInput a draft is typing into, as the page shows it (its changeAction).
const TextInput* inputOf(const WindowScene& s, const Draft& d)
{
    const Placed* p = findField(s, [&](const Placed& p) {
        auto* input = std::get_if<TextInput>(&p.item);
        return input && input->actionId == d.actionId && !input->inPlace;
    });
    return p ? &std::get<TextInput>(p->item) : nullptr;
}

Action sent(const WindowScene& s, const std::string& actionId, const std::string& text)
{
    return Action{ .providerId = s.providerId, .actionId = actionId, .text = text, .windowId = s.windowId };
}

// A live field reports every edit (decision 079), not just Enter.
std::optional<Action> liveEdit(const WindowScene& s, const Draft& d, const std::string& before)
{
    if (d.editing || d.text == before) return std::nullopt;
    const TextInput* input = inputOf(s, d);
    if (!input || input->changeAction.empty()) return std::nullopt;
    return sent(s, input->changeAction, d.text);
}

std::vector<std::string> linesOf(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.find_first_not_of(" \t") != std::string::npos) lines.push_back(line);
    }
    return lines;
}

} // namespace

bool isField(const WindowScene& s, const std::string& key)
{
    const Placed* p = findOnPage(s, key);
    if (!p) return false;
    if (std::holds_alternative<TextInput>(p->item)) return true;
    auto* text = std::get_if<Text>(&p->item);
    return text && !text->editAction.empty();
}

std::optional<Action> focusField(WindowTyping& typing, const WindowScene& s, const std::string& key, std::optional<std::size_t> caret)
{
    const Placed* p = findOnPage(s, key);
    if (!p || !isField(s, key)) return std::nullopt;
    Draft d{ .providerId = s.providerId, .activityId = s.windowId };
    if (auto* input = std::get_if<TextInput>(&p->item)) {
        if (input->focused && typing.draft) { // it has the keys already: only the caret moves
            if (caret) placeCaret(*typing.draft, *caret, false);
            return std::nullopt;
        }
        d.actionId = input->actionId;
        d.maxLength = input->maxLength;
        d.leaveAction = input->leaveAction;
        if (input->inPlace) d.text = input->value, d.editing = true; // a text drawn while edited: an edit again
    } else {
        const Text& text = std::get<Text>(p->item);
        d.actionId = text.editAction;
        d.text = text.value;
        d.editing = true;
        d.maxLength = text.editMaxLength;
        caret.reset(); // starting an edit: all of it selected (decision 122), wherever it was clicked
    }
    std::optional<Action> was = blurField(typing, s);
    if (!d.editing) { // the text it was left with (decision 050)
        auto kept = typing.kept.find(keptKey(s.windowId, d.actionId));
        if (kept != typing.kept.end()) d.text = std::move(kept->second), typing.kept.erase(kept);
        else if (auto* input = std::get_if<TextInput>(&p->item)) d.text = input->value; // what it shows: a question's field's start (decision 142)
    }
    placeCaret(d, caret.value_or(d.text.size()), false);
    if (d.editing && !std::holds_alternative<TextInput>(p->item)) d.anchor = 0; // the whole text selected: typing replaces it
    typing.draft = std::move(d);
    return was;
}

std::optional<Action> blurField(WindowTyping& typing, const WindowScene& s)
{
    if (!typing.draft) return std::nullopt;
    Draft d = std::move(*typing.draft);
    typing.draft.reset();
    if (d.editing) return sent(s, d.actionId, d.text); // kept, like any text box (decision 043)
    std::string key = keptKey(s.windowId, d.actionId);
    if (!d.leaveAction.empty()) { // a row being written: left, it's added if there's anything, and goes (decisions 127, 132)
        typing.kept.erase(key);
        return sent(s, d.leaveAction, d.text);
    }
    if (d.text.empty()) typing.kept.erase(key);
    else typing.kept[key] = d.text;
    return std::nullopt;
}

FieldOutcome fieldKey(WindowTyping& typing, const WindowScene& s, FieldKey key, bool ctrl, bool shift)
{
    if (!typing.draft) return {};
    Draft& d = *typing.draft;
    std::string before = d.text;
    switch (key) {
    case FieldKey::Enter: {
        if (d.editing) { // an edit ends with Enter: sent, and the text is plain text again
            Action edit = sent(s, d.actionId, d.text);
            typing.draft.reset();
            return { edit, true };
        }
        std::optional<Action> send = d.text.empty() ? std::nullopt : std::optional<Action>(sent(s, d.actionId, d.text));
        d.text.clear(); // a field keeps the keys: type the next one
        d.caret = d.anchor = 0;
        return { send, false };
    }
    case FieldKey::Escape: // stops typing; an edit is dropped, a field's text kept
        if (!d.leaveAction.empty()) { // a row being written: dropped, and it goes (decision 132)
            Action drop = sent(s, d.leaveAction, "");
            typing.draft.reset();
            return { drop, true };
        }
        if (d.editing) typing.draft.reset();
        else blurField(typing, s);
        return { std::nullopt, true };
    case FieldKey::Backspace: eraseAtCaret(d, false, ctrl); break;
    case FieldKey::Delete: eraseAtCaret(d, true, ctrl); break;
    case FieldKey::Left: moveCaret(d, ctrl ? CaretMove::WordLeft : CaretMove::Left, shift); break;
    case FieldKey::Right: moveCaret(d, ctrl ? CaretMove::WordRight : CaretMove::Right, shift); break;
    case FieldKey::Home: moveCaret(d, CaretMove::Start, shift); break;
    case FieldKey::End: moveCaret(d, CaretMove::End, shift); break;
    case FieldKey::SelectAll: selectAll(d); break;
    }
    return { liveEdit(s, d, before), false };
}

FieldOutcome typeText(WindowTyping& typing, const WindowScene& s, const std::string& text)
{
    if (!typing.draft) return {};
    Draft& d = *typing.draft;
    std::vector<std::string> lines = linesOf(text);
    bool oneLine = text.find('\n') == std::string::npos;
    if (!oneLine && lines.size() > 1 && !d.editing) return { sent(s, d.actionId, text), false }; // a to-do each
    std::string before = d.text;
    if (oneLine) insertAtCaret(d, text);
    else if (!lines.empty()) insertAtCaret(d, lines.front());
    return { liveEdit(s, d, before), false };
}

void selectWord(WindowTyping& typing, std::size_t at)
{
    if (typing.draft) selectWordAt(*typing.draft, at);
}

std::optional<Action> focusByAction(WindowTyping& typing, const WindowScene& s, const std::string& action)
{
    for (const Placed& p : s.page) {
        auto* input = std::get_if<TextInput>(&p.item);
        auto* text = std::get_if<Text>(&p.item);
        bool named = (input && input->actionId == action) || (text && !text->editAction.empty() && text->editAction == action);
        if (!named) continue;
        std::optional<Action> left = focusField(typing, s, p.key, std::nullopt);
        if (typing.draft) selectAll(*typing.draft);
        return left;
    }
    return std::nullopt;
}

void selectAllText(WindowTyping& typing)
{
    if (typing.draft) selectAll(*typing.draft);
}

void moveCaretTo(WindowTyping& typing, std::size_t at, bool select)
{
    if (typing.draft) placeCaret(*typing.draft, at, select);
}

bool dropGoneField(WindowTyping& typing, const WindowScene& s)
{
    if (!typing.draft) return false;
    if (focusedField(s)) return false;
    if (typing.draft->editing) typing.draft.reset(); // the text it edited went (deleted, another page): nothing to send
    else blurField(typing, s);
    return true;
}

bool focusArrived(WindowTyping& typing, const WindowScene& s)
{
    if (typing.draft) return false; // something has the keys: it keeps them
    for (const Placed& p : s.page)
        if (auto* input = std::get_if<TextInput>(&p.item); input && input->autoFocus && !input->inPlace) {
            focusField(typing, s, p.key, std::nullopt); // nothing had the keys: nothing to send
            return true;
        }
    return false;
}

const Placed* focusedField(const WindowScene& s)
{
    return findField(s, [](const Placed& p) {
        auto* input = std::get_if<TextInput>(&p.item);
        return input && input->focused;
    });
}

std::optional<Action> dialogTakesKeys(WindowTyping& typing, const WindowScene& s)
{
    if (!s.dialog) return std::nullopt;
    for (const Placed& p : s.dialog->elements) {
        auto* input = std::get_if<TextInput>(&p.item);
        if (!input || !input->autoFocus) continue;
        if (typing.draft && typing.draft->actionId == input->actionId) return std::nullopt; // it has them already
        std::optional<Action> left = blurField(typing, s); // what was being typed on the page lets go first
        // Its text left there (a click on its buttons took the keys) comes back with them; a new
        // question starts from its own, since answering forgets it (dialogFieldText).
        focusField(typing, s, p.key, std::nullopt);
        if (typing.draft) selectAll(*typing.draft); // typed over at once
        return left;
    }
    return std::nullopt;
}

std::string dialogFieldText(WindowTyping& typing, const WindowScene& s)
{
    std::string key = keptKey(s.windowId, kDialogField);
    std::string text;
    if (typing.draft && typing.draft->actionId == kDialogField) {
        text = typing.draft->text;
        typing.draft.reset();
    } else if (auto kept = typing.kept.find(key); kept != typing.kept.end()) {
        text = kept->second;
    } else if (s.dialog) {
        for (const Placed& p : s.dialog->elements)
            if (auto* input = std::get_if<TextInput>(&p.item); input && input->actionId == kDialogField) text = input->value;
    }
    typing.kept.erase(key);
    return text;
}

} // namespace cup
