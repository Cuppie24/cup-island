#pragma once
// What can sit over the shown card: text being typed into its field
// (decision 016) and a provider's dialog (decision 020). Both belong to the
// card they were started on and go with it. Part of reduce(); split out of
// island.cpp to keep it readable.

#include "core/island.h"

namespace cup {

void focusInput(IslandState& state, const InputFocused& e, Ms now); // only on the shown card, not under a dialog
void submitText(IslandState& state, Ms now); // the field empties and keeps focus
void showDialog(IslandState& state, const DialogShown& e, Ms now); // only over the shown card; typing stops
void openMenu(IslandState& state, const MenuOpened& e, Ms now);    // only over the shown card, never under a dialog

// After every event: a draft, dialog, list drag or focus request whose card isn't the one shown any more is dropped.
// A field's unsent text is kept (IslandState::keptText, decision 050).
void dropStaleOverlays(IslandState& state);

// The draft goes (Esc, the card closing, another field focused): a field's
// unsent text is set aside for when it's focused again (decision 050).
// Text being edited in place isn't: the app has sent it or it was dropped.
void dropDraft(IslandState& state);

std::string keptKey(const std::string& activityId, const std::string& actionId); // "<activityId>/<actionId>"

} // namespace cup
