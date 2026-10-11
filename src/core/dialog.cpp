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
    return s.dialog && s.dialog->activityId == activityId ? &*s.dialog : nullptr;
}

} // namespace cup
