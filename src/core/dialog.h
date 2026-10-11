#pragma once
// The host's dialog (decision 019): what it shows in place of a card's
// content while a provider's question is open. Pure.

#include "core/island.h"

namespace cup {

// The dialog's own buttons. Reserved action names: the app answers the
// dialog instead of routing them to the card's provider.
inline constexpr const char* kDialogConfirm = "dialog.confirm";
inline constexpr const char* kDialogCancel = "dialog.cancel";

// Title, message, then Cancel and the confirm button, centred.
Content dialogContent(const Dialog& dialog);

// The dialog over this card, if one is open.
const Dialog* dialogOver(const IslandState& state, const std::string& activityId);

} // namespace cup
