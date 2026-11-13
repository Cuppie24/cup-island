#pragma once
// Typing in a provider's window (decision 105): its text fields and the
// texts edited in place, on the island's own rules (decisions 016, 043, 045,
// 050). One field at a time per window has the keys; what a key does to it
// is text_edit's, what's sent is the same Action a card's field sends, with
// Action::windowId set. Pure: the app keeps a WindowTyping per window and
// turns WM_CHAR and keys into these calls.

#include "core/window_layout.h"

#include <map>
#include <optional>
#include <string>

namespace cup {

struct WindowTyping {
    // The field with the keys. Draft::activityId is the window's id, so
    // windowLayout fills that TextInput from it, as layout() does a card's.
    std::optional<Draft> draft;
    // Fields' unsent text while they don't have the keys (decision 050), by keptKey(windowId, actionId).
    std::map<std::string, std::string> kept;
    bool operator==(const WindowTyping&) const = default;
};

// The page's element `key`, if it's something to type in: a TextInput, or
// a Text with an editAction (edited in place, decision 043).
bool isField(const WindowScene& scene, const std::string& key);

// `key` gets the keys, its caret at `caret` (a click's byte offset; none:
// the end). A field gets the text it was left with; a Text edited in place
// starts from its own. Whatever had the keys before lets go (blurField).
// Nothing for a key that isn't a field.
std::optional<Action> focusField(WindowTyping& typing, const WindowScene& scene, const std::string& key, std::optional<std::size_t> caret);

// The field lets go of the keys: a field's text is kept for later (decision
// 050); a Text edited in place is done, and its new text is sent, like a
// click elsewhere on a card. The Action to route, if any.
std::optional<Action> blurField(WindowTyping& typing, const WindowScene& scene);

// A field's keys, as a text box's (decision 043). Not: everything else
// (Tab, Up and Down, Ctrl+Tab, Ctrl+W), which the window handles as usual
// after the field lets go.
enum class FieldKey { Enter, Escape, Backspace, Delete, Left, Right, Home, End, SelectAll };

struct FieldOutcome {
    std::optional<Action> send; // to route: Enter's text, a live field's change (decision 079), an edit
    bool letGo = false;         // the field doesn't have the keys any more (Esc, an edit sent)
};

// A key in the focused field. `ctrl`: by word (arrows, Backspace, Delete); `shift`: selecting.
FieldOutcome fieldKey(WindowTyping& typing, const WindowScene& scene, FieldKey key, bool ctrl, bool shift);

// Typed or pasted text: one line goes in at the caret (past maxLength, what
// fits). Several lines pasted into a field are sent whole, as on a card
// (the provider splits them: a to-do each, decision 016); edited in place,
// only the first goes in.
FieldOutcome typeText(WindowTyping& typing, const WindowScene& scene, const std::string& text);

// A double click selects a word, a triple click everything; a Shift+click
// or a drag moves the caret, keeping the selection's other end.
void selectWord(WindowTyping& typing, std::size_t at);
void selectAllText(WindowTyping& typing);
void moveCaretTo(WindowTyping& typing, std::size_t at, bool select);

// After the window's content changed: the field with the keys is still on
// the page, or it lets go (its text kept, an edit dropped). True if it went.
bool dropGoneField(WindowTyping& typing, const WindowScene& scene);

// The focused field on the page, as windowLayout filled it (caret and all); none if nothing has the keys.
const Placed* focusedField(const WindowScene& scene);

} // namespace cup
