#include "core/dialog.h"

namespace cup {

Content dialogContent(const Dialog& d)
{
    Content c{ .label = d.title, .card = CardLayout::Centred };
    c.expandedCenter = {
        row({ Text{ .value = d.title, .style = TextStyle::Title, .centred = true } }),
        row({ Text{ .value = d.message, .style = TextStyle::Body, .centred = true } }),
        Row{ .items = { Button{ .actionId = kDialogCancel, .label = "Cancel" },
                        Button{ .actionId = kDialogConfirm, .label = d.confirmLabel, .destructive = d.destructive } },
             .align = Align::Center },
    };
    return c;
}

const Dialog* dialogOver(const IslandState& s, const std::string& activityId)
{
    if (s.dialog && s.dialog->activityId == activityId) return &*s.dialog;
    for (const FloatingWindow& w : s.windows) // a window's own (decision 081)
        if (w.activityId == activityId && w.dialog) return &*w.dialog;
    return nullptr;
}

} // namespace cup
