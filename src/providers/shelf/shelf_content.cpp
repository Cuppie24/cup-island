#include "providers/shelf/shelf_content.h"

#include "providers/shelf/drop_plan.h"

namespace cup::shelf {
namespace {

std::string things(std::size_t n) { return std::to_string(n) + (n == 1 ? " thing" : " things"); }

// Its kind's glyph, until (or unless) Windows gives a picture.
Glyph kindGlyph(const Entry& item)
{
    switch (item.what) {
    case DragKind::Folder: return Glyph::Folder;
    case DragKind::Text: return Glyph::Edit;
    case DragKind::Url: return Glyph::Link;
    case DragKind::File: break;
    }
    return Glyph::File;
}

// What it is, or how it's doing: under its name.
std::string status(const Entry& item)
{
    switch (item.state) {
    case State::Ready: {
        std::string kind = item.kind == Kind::Link ? "Link" : "Copy";
        return item.bytes ? kind + " · " + formatBytes(item.bytes) : kind;
    }
    case State::Copying: return "Copying…";
    case State::Failed: return item.problem;
    case State::Missing: return "Not found";
    }
    return "";
}

// Shown on hover only (decision 063): Show in folder while it's there to
// show, Keep as link for a failed copy with a source, and Remove.
std::vector<cup::Item> actionsFor(const Entry& item)
{
    std::string id = std::to_string(item.id);
    std::vector<cup::Item> actions;
    // Backed: over a thumbnail of their own colour they'd vanish (decision 063).
    if (item.state == State::Ready && !item.path.empty())
        actions.push_back(Button{ .glyph = Glyph::Reveal, .actionId = action::kReveal + id, .compact = true, .revealOnHover = true, .backed = true });
    if (item.state == State::Failed && !item.source.empty())
        actions.push_back(Button{ .actionId = action::kKeep + id, .label = "Keep as link", .compact = true, .revealOnHover = true, .backed = true });
    actions.push_back(Button{ .glyph = Glyph::Delete, .actionId = action::kRemove + id, .compact = true, .revealOnHover = true, .backed = true });
    return actions;
}

Text nameOf(const Entry& item, TextStyle style)
{
    // Renamed in place; a copy still copying has no file to rename yet.
    std::string edit = item.state == State::Copying ? "" : action::kRename + std::to_string(item.id);
    return Text{ .value = shownName(item), .style = style, .editAction = edit, .editMaxLength = 120, .editHint = !edit.empty() };
}

std::string opens(const Entry& item) { return item.state == State::Ready ? action::kOpen + std::to_string(item.id) : ""; }

std::shared_ptr<const ImageData> pictureOf(const Entry& item, const Thumbnails& thumbnails)
{
    auto it = thumbnails.find(item.id);
    return it == thumbnails.end() ? nullptr : it->second;
}

// A link or a copy, at a glance (decision 061): a small badge by "Link" or "Copy".
Icon badgeOf(const Entry& item)
{
    Glyph glyph = item.kind == Kind::Link ? Glyph::Link : Glyph::Copy;
    if (item.state == State::Failed || item.state == State::Missing) glyph = Glyph::Warning;
    return Icon{ .glyph = glyph, .compact = true };
}

// Under its name: the badge and what it is, or a copy's progress.
Row statusRow(const Entry& item)
{
    if (item.state == State::Copying)
        return row({ Progress{ .timeline = { .position = Ms{ static_cast<long long>(item.progress * 1000) }, .duration = Ms{ 1000 } } } });
    return row({ badgeOf(item), Text{ .value = status(item), .style = TextStyle::Caption } });
}

Cell cellFor(const Entry& item, const Thumbnails& thumbnails)
{
    Row picture = row({ Image{ .data = pictureOf(item, thumbnails), .role = ImageRole::Thumbnail, .contain = true } });
    Row name = row({ nameOf(item, TextStyle::Body) });
    Row under = statusRow(item);
    // No buttons on a card: its right-click menu has them (decision 066).
    return { .id = "item-" + std::to_string(item.id), .rows = { picture, name, under }, .openAction = opens(item) };
}

Row rowFor(const Entry& item, const Thumbnails& thumbnails)
{
    Row r{ .list = action::kList, .id = "item-" + std::to_string(item.id), .listItem = true, .openAction = opens(item), .selectable = true };
    if (auto picture = pictureOf(item, thumbnails)) r.items.push_back(Image{ .data = picture, .role = ImageRole::Icon, .contain = true });
    else r.items.push_back(Icon{ kindGlyph(item) });
    r.items.push_back(nameOf(item, TextStyle::Body)); // the rest of the row: the name gets the room (decision 064)
    if (item.state == State::Copying) r.items.push_back(Progress{ .timeline = { .position = Ms{ static_cast<long long>(item.progress * 1000) }, .duration = Ms{ 1000 } } });
    else {
        r.items.push_back(badgeOf(item));
        r.items.push_back(Text{ .value = status(item), .style = TextStyle::Caption, .column = true });
    }
    r.trailing = actionsFor(item);
    return r;
}

// The last card: Windows' file picker, the files kept as copies (decision 064).
Cell addCell()
{
    return { .id = "add",
             .rows = { row({ Button{ .glyph = Glyph::Plus, .actionId = action::kAdd } }, Align::Center),
                       row({ Text{ .value = "Add files", .style = TextStyle::Caption, .centred = true } }, Align::Center) },
             .fixed = true,
             .clickAnywhere = true }; // the whole card is the button, lit as one
}

std::string countOf(std::size_t n) { return n == 1 ? "1 item" : std::to_string(n) + " items"; }

} // namespace

std::optional<std::uint64_t> itemOfRow(const std::string& rowId)
{
    if (!rowId.starts_with("item-")) return std::nullopt;
    try {
        return std::stoull(rowId.substr(5));
    } catch (...) {
        return std::nullopt;
    }
}

Panel shelfPanel(const Items& items, View view, const Thumbnails& thumbnails, std::size_t picked)
{
    const auto& all = items.all();
    std::string glance = all.empty() ? "Drop files on the island" : things(all.size());
    Content content{
        .label = "Shelf",
        .card = CardLayout::Large,
        .keepOpen = true,    // a near miss between its things doesn't close it
        .steadyPress = true, // a page of things to click: the island doesn't squash under a press (decision 063)
        .deleteAction = action::kDeleteSelected, // the Delete key, with things picked (decision 064)
        .tileLeading = { row({ Icon{ Glyph::Package } }, Align::Center) },
        .tile = { row({ Text{ "Shelf", TextStyle::Title } }), row({ Text{ glance, TextStyle::Caption } }) },
    };
    Slot& card = content.expandedCenter;
    Button toggle{ .glyph = view == View::Cards ? Glyph::ViewList : Glyph::ViewGrid, .actionId = action::kView, .compact = true };
    std::string summary = things(all.size());
    if (std::uint64_t total = items.totalBytes()) summary += " · " + formatBytes(total);
    card.push_back(Row{ .items = { Text{ .value = "Shelf", .style = TextStyle::Heading }, Text{ .value = all.empty() ? "" : summary, .style = TextStyle::Caption, .alignEnd = true } },
                        .trailing = { toggle } });
    if (view == View::Cards) {
        Row grid{ .list = action::kList, .id = "grid", .selectable = true };
        for (auto it = all.rbegin(); it != all.rend(); ++it) grid.cells.push_back(cellFor(*it, thumbnails)); // the newest first
        grid.cells.push_back(addCell());
        card.push_back(grid);
    } else {
        for (auto it = all.rbegin(); it != all.rend(); ++it) card.push_back(rowFor(*it, thumbnails));
        card.push_back(Row{ .items = { Button{ .glyph = Glyph::Plus, .actionId = action::kAdd, .label = "Add files", .compact = true, .withGlyph = true } },
                            .align = Align::Center, .clickAnywhere = true, .list = action::kList, .id = "add", .listItem = true }); // the whole row
    }
    // Under the list, which fills the card: always at its foot, however far it's scrolled (decision 063).
    // Something picked: it deletes that (decision 064); else it clears the shelf.
    if (picked > 0)
        card.push_back(Row{ .items = { Button{ .glyph = Glyph::Delete, .actionId = action::kDeleteSelected, .label = "Delete " + countOf(picked),
                                               .compact = true, .destructive = true, .withGlyph = true } },
                            .align = Align::Center });
    else if (!all.empty())
        card.push_back(Row{ .items = { Button{ .glyph = Glyph::Delete, .actionId = action::kClear, .label = "Clear", .compact = true, .withGlyph = true } },
                            .align = Align::Center });
    return { .id = kPanelId, .content = content };
}

Dialog removeQuestion(const Entry& item)
{
    std::string message = item.kind == Kind::Link ? "It goes from the shelf; the original stays where it is." : "Its copy on the shelf is deleted.";
    return { .activityId = kPanelId, .title = "Remove \"" + shownName(item) + "\"?", .message = message, .confirmLabel = "Remove",
             .confirmAction = action::kRemoveConfirmed + std::to_string(item.id), .destructive = true };
}

Dialog deleteQuestion(const Items& items, const std::vector<std::uint64_t>& ids)
{
    bool links = false;
    for (std::uint64_t id : ids)
        if (const Entry* item = items.find(id); item && item->kind == Kind::Link) links = true;
    std::string message = countOf(ids.size()) + " will be removed from the shelf." + (links ? " Originals are kept." : "");
    return { .activityId = kPanelId, .title = "Delete " + countOf(ids.size()) + "?", .message = message, .confirmLabel = "Delete",
             .confirmAction = action::kDeleteSelectedConfirmed, .destructive = true };
}

Dialog clearQuestion(const Items& items)
{
    std::string message = things(items.all().size()) + " will be removed from the shelf.";
    if (items.anyLinks()) message += " Originals are kept.";
    return { .activityId = kPanelId, .title = "Clear the shelf?", .message = message,
             .confirmLabel = "Clear", .confirmAction = action::kClearConfirmed, .destructive = true };
}

} // namespace cup::shelf
