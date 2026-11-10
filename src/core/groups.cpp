#include "core/groups.h"

namespace cup {

Text itemText(std::string name, std::string description)
{
    return Text{ .value = std::move(name), .detail = std::move(description), .detailTrimsEnd = true };
}

Row groupItem(Text text, std::vector<Item> end)
{
    bool clicks = false;
    for (const Item& item : end) clicks = clicks || std::holds_alternative<Button>(item) || std::holds_alternative<Switch>(item);
    Row r{ .items = { std::move(text) } };
    for (Item& item : end) r.items.push_back(std::move(item));
    r.clickAnywhere = clicks;
    r.listItem = clicks; // lit under the pointer: it says the row is clickable
    r.grouped = true;
    return r;
}

Row groupLabel(std::string label, bool first)
{
    return Row{ .items = { Text{ .value = std::move(label), .style = TextStyle::Heading } }, .section = !first };
}

void addGroup(Slot& slot, const std::string& label, std::vector<Row> rows, const std::string& list)
{
    if (rows.empty()) return;
    // Unlabelled after other rows: the group's first row carries the gap, so groups stay apart.
    if (!label.empty()) slot.push_back(groupLabel(label, slot.empty()));
    else rows.front().section = !slot.empty();
    for (Row& r : rows) {
        r.grouped = true;
        if (!list.empty()) r.list = list;
        slot.push_back(std::move(r));
    }
    if (!label.empty() && !list.empty()) slot[slot.size() - rows.size() - 1].list = list;
}

} // namespace cup
