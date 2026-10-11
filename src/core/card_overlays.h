#pragma once
// What can sit over the shown card: text being typed into its field
// (decision 016) and a provider's dialog (decision 019). Both belong to the
// card they were started on and go with it. Part of reduce(); split out of
// island.cpp to keep it readable.

#include "core/island.h"

namespace cup {

void focusInput(IslandState& state, const InputFocused& e, Ms now); // only on the shown card, not under a dialog
void typeText(IslandState& state, const TextTyped& e, Ms now);
void eraseText(IslandState& state, Ms now);  // one whole UTF-8 character
void submitText(IslandState& state, Ms now); // the field empties and keeps focus
void showDialog(IslandState& state, const DialogShown& e, Ms now); // only over the shown card; typing stops

// After every event: a draft or dialog whose card isn't the one shown any more is dropped.
void dropStaleOverlays(IslandState& state);

} // namespace cup
