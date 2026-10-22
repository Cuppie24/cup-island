#include "core/toast.h"

namespace cup {

const Toast* toastOn(const IslandState& s, const std::string& activityId)
{
    return s.toast && s.toast->activityId == activityId ? &*s.toast : nullptr;
}

Content withToast(Content card, const Toast& toast)
{
    Row note{ .items = { Text{ .value = toast.text, .style = TextStyle::Caption, .centred = true } }, .align = Align::Center, .hug = true,
              .apart = true };
    if (!toast.actionLabel.empty()) note.items.push_back(Button{ .actionId = toast.actionId, .label = toast.actionLabel, .compact = true });
    card.expandedCenter.push_back(note);
    if (!card.sizedLike.empty()) card.sizedLike.push_back(note); // a page: the card grows the same
    return card;
}

} // namespace cup
