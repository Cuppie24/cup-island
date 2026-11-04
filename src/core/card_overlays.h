#pragma once
// What can sit over the shown card: text being typed into its field
// (decision 016) and a provider's dialog (decision 020). Both belong to the
// card they were started on and go with it. Part of reduce(); split out of
// island.cpp to keep it readable.

#include "core/island.h"
#include "core/layout.h"
#include "core/provider.h"

namespace cup {

void focusInput(IslandState& state, const InputFocused& e, Ms now); // only on the shown card, not under a dialog
void submitText(IslandState& state, Ms now); // the field empties and keeps focus
void showDialog(IslandState& state, const DialogShown& e, Ms now); // only over a card on screen; typing on it stops
// A window's dialog and toast are its own (decision 081): shown over the
// card's window if it has one, and answered in the focused window.
void showToast(IslandState& state, const ToastShown& e, Ms now);
void closeDialog(IslandState& state);
void closeToast(IslandState& state);

// After every event: a draft, dialog or list drag whose card isn't the one shown any more is dropped.
// A field's unsent text is kept (IslandState::keptText, decision 050), but not
// a fieldFirst card's: it opens empty (decision 079).
void dropStaleOverlays(IslandState& state);

// A shown card that is its field (Content::fieldFirst, decision 079): its
// field gets the keys, with any kept text (while the card stays open), unless something
// else has them (another field, a dialog). Not a touch: the idle timeout
// still counts from the last real one.
void focusFieldFirst(IslandState& state);

// The draft goes (Esc, the card closing, another field focused): a field's
// unsent text is set aside for when it's focused again (decision 050).
// Text being edited in place isn't: the app has sent it or it was dropped.
void dropDraft(IslandState& state);

std::string keptKey(const std::string& activityId, const std::string& actionId); // "<activityId>/<actionId>"

// A live field's edit (TextInput::changeAction, decision 079): the action
// to send after an event, if the same field's text changed. `scene` is the
// new one; a field not on it, or not live, sends nothing. Focusing a field,
// or leaving one, isn't an edit.
std::optional<Action> liveChange(const std::optional<Draft>& before, const std::optional<Draft>& after, const Scene& scene);

} // namespace cup
