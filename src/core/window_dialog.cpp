#include "core/window_dialog.h"

#include "core/dialog.h"
#include "core/slot_layout.h"
#include "core/tokens.h"
#include "core/window_page.h"

#include <algorithm>
#include <cmath>

namespace cup {

WindowDialogScene placeWindowDialog(const Dialog& d, Vec2 size, float top, const Source& source, Ms now, const MeasureText& measure)
{
    const Tokens& t = tokens();
    WindowDialogScene scene;
    scene.scrim = { 0, top, size.x, std::max(0.0f, size.y - top) };
    // The island's dialog's rows (dialogContent), in the window's type (decision 112): the message wraps.
    Slot rows{ row({ Text{ .value = d.title, .style = TextStyle::Title } }),
               Row{ .items = { Text{ .value = d.message, .style = TextStyle::Caption, .wrap = true } } },
               Row{ .items = { Button{ .actionId = kDialogCancel, .label = "Cancel", .compact = true },
                               Button{ .actionId = kDialogConfirm, .label = d.confirmLabel, .compact = true, .destructive = d.destructive } },
                    .align = Align::End,
                    .apart = true } };
    if (d.message.empty()) rows.erase(rows.begin() + 1);
    // Its icon (decision 165): over the title, a Home widget's size.
    if (d.glyph) rows.insert(rows.begin(), row({ Icon{ .glyph = *d.glyph, .large = true } }));
    // Its field (decision 142): above the buttons, with the keys as it appears (window_fields: dialogTakesKeys).
    if (d.field)
        rows.insert(rows.end() - 1, Row{ .items = { TextInput{ .actionId = kDialogField, .value = *d.field, .maxLength = d.fieldMaxLength,
                                                                .autoFocus = true } } });
    rows = windowStyled(std::move(rows));
    float pad = t.windowDialogPadding;
    float width = std::round(std::min(t.windowDialogWidth, std::max(0.0f, size.x - 2 * t.windowPagePadding.x)));
    Rect inner{ 0, 0, std::max(0.0f, width - 2 * pad), 0 };
    inner.height = slotHeight(rows, inner, source, measure);
    float height = std::round(inner.height + 2 * pad);
    scene.panel = { std::round((size.x - width) / 2), std::round(top + (scene.scrim.height - height) / 2), width, height };
    // Its corners nest its buttons' (decision 123): their radius, the window's own (windowNavRadius), plus its padding.
    scene.radius = std::min(pad + t.windowNavRadius, height / 2);
    inner.x = scene.panel.x + pad;
    inner.y = scene.panel.y + pad;
    placeSlot({ "dialog", &rows, inner }, source, std::max(0.0f, scene.radius - pad), now, scene.elements, measure);
    return scene;
}

} // namespace cup
