#include "providers/shelf/shelf_content.h"

namespace cup::shelf {
namespace {

std::string things(std::size_t n) { return std::to_string(n) + (n == 1 ? " thing" : " things"); }

Row itemRow(const Item& item)
{
    std::string id = std::to_string(item.id);
    Glyph glyph = item.kind == Kind::Link ? Glyph::Link : Glyph::Copy;
    if (item.state == State::Failed || item.state == State::Missing) glyph = Glyph::Warning;
    Button remove{ .glyph = Glyph::Delete, .actionId = action::kRemove + id, .compact = true, .revealOnHover = true };
    Row row{ .list = action::kList, .id = "item-" + id, .listItem = true };
    row.items.push_back(Icon{ glyph });
    switch (item.state) {
    case State::Ready:
        row.items.push_back(Text{ .value = item.name });
        row.trailing = { Button{ .glyph = Glyph::OpenApp, .actionId = action::kOpen + id, .compact = true }, remove };
        break;
    case State::Copying: {
        // A still bar at the copy's fraction: republished as it moves.
        Timeline at{ .position = Ms{ static_cast<long long>(item.progress * 1000) }, .duration = Ms{ 1000 } };
        row.items.push_back(Text{ .value = item.name });
        row.items.push_back(Progress{ .timeline = at });
        row.trailing = { remove };
        break;
    }
    case State::Failed:
        row.items.push_back(Text{ .value = item.name + " · " + item.problem, .style = TextStyle::Caption });
        if (!item.source.empty()) row.trailing.push_back(Button{ .actionId = action::kKeep + id, .label = "Keep as link", .compact = true });
        row.trailing.push_back(remove);
        break;
    case State::Missing:
        row.items.push_back(Text{ .value = item.name + " · Not found", .style = TextStyle::Caption });
        row.trailing = { remove };
        break;
    }
    return row;
}

} // namespace

Panel shelfPanel(const Items& items)
{
    const auto& all = items.all();
    std::string glance = all.empty() ? "Drop files on the island" : things(all.size());
    Content content{
        .label = "Shelf",
        .card = CardLayout::Centred,
        .keepOpen = true, // a near miss between its rows doesn't close it
        .tileLeading = { row({ Icon{ Glyph::Download } }, Align::Center) },
        .tile = { row({ Text{ "Shelf", TextStyle::Title } }), row({ Text{ glance, TextStyle::Caption } }) },
    };
    Slot& card = content.expandedCenter;
    card.push_back(row({ Text{ .value = "Shelf", .style = TextStyle::Heading } }));
    if (all.empty()) {
        card.push_back(row({ Text{ .value = "Drag files, text or links onto the island", .style = TextStyle::Caption, .centred = true } }, Align::Center));
        return { .id = kPanelId, .content = content };
    }
    for (auto it = all.rbegin(); it != all.rend(); ++it) card.push_back(itemRow(*it)); // the newest on top
    card.push_back(Row{ .items = { Button{ .actionId = action::kClear, .label = "Clear", .compact = true } }, .align = Align::Center });
    return { .id = kPanelId, .content = content };
}

Dialog clearQuestion(const Items& items)
{
    std::string message = things(items.all().size()) + " will be removed from the shelf.";
    if (items.anyLinks()) message += " Originals are kept.";
    return { .activityId = kPanelId, .title = "Clear the shelf?", .message = message,
             .confirmLabel = "Clear", .confirmAction = action::kClearConfirmed, .destructive = true };
}

} // namespace cup::shelf
