#include "core/side_card.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

const Item* firstItem(const Slot& slot)
{
    for (const Row& row : slot)
        if (!row.items.empty()) return &row.items.front();
    return nullptr;
}

// Title: the compact centre's text (media's swimming "Song - Artist"), else the activity's name.
Text titleOf(const Content& c)
{
    const Item* item = firstItem(c.compactCenter);
    if (auto* text = item ? std::get_if<Text>(item) : nullptr)
        return Text{ .value = text->value, .style = TextStyle::Body, .marquee = text->marquee };
    return Text{ .value = c.label, .style = TextStyle::Body, .marquee = true };
}

// Status: the compact trailing item if it's short text ("4:59") or a glyph-sized item (waveform).
std::optional<Item> statusOf(const Content& c)
{
    const Item* item = firstItem(c.compactTrailing);
    if (!item) return std::nullopt;
    if (auto* text = std::get_if<Text>(item)) return Text{ text->value, TextStyle::Caption, true };
    if (std::holds_alternative<Icon>(*item) || std::holds_alternative<Waveform>(*item)) return *item;
    return std::nullopt;
}

// One row: lead square, title, status. Keys carry the activity id, so a row's
// content stays the same visual when the list reorders.
void placeRow(const Activity& a, Rect row, float nestedRadius, std::vector<Placed>& out)
{
    const Tokens& t = tokens();
    float inset = t.compactInset; // the same padding as a compact square
    float side = row.height - 2 * inset, x = row.x + inset, right = row.x + row.width - inset;
    auto place = [&](const std::string& part, Item item, Rect frame, float radius = 0) {
        out.push_back({ .key = "side/" + a.id + "/" + part, .item = std::move(item), .frame = frame, .hitFrame = frame,
                        .cornerRadius = radius, .providerId = a.providerId, .activityId = a.id });
    };

    const Item* lead = firstItem(a.content.compactLeading);
    if (!lead) lead = firstItem(a.content.minimal);
    if (lead) {
        Rect square{ x, row.y + inset, side, side };
        if (std::holds_alternative<Image>(*lead)) {
            place("lead", *lead, square, std::min(nestedRadius, side / 2));
        } else {
            float g = std::min(t.iconSize, side); // glyphs: icon-sized, centred in the square
            place("lead", *lead, { x + (side - g) / 2, row.y + (row.height - g) / 2, g, g });
        }
    }
    x += side + t.itemSpacing;

    if (auto status = statusOf(a.content)) {
        bool text = std::holds_alternative<Text>(*status);
        float w = text ? t.sideStatusWidth : t.iconSize, h = text ? row.height : t.iconSize;
        place("status", *status, { right - w, row.y + (row.height - h) / 2, w, h });
        right -= w + t.itemSpacing;
    }
    place("title", titleOf(a.content), { x, row.y, std::max(0.0f, right - x), row.height });
}

} // namespace

std::optional<SideCard> sideCardFor(const IslandState& s, Rect card)
{
    if (mode(s) != Mode::Expanded || s.activities.size() < 2) return std::nullopt;
    const Tokens& t = tokens();

    std::vector<const Activity*> others;
    for (const Activity& a : s.activities)
        if (a.id != s.expandedId) others.push_back(&a);
    std::size_t maxRows = std::max<std::size_t>(t.sideCardRows, 1);
    bool overflow = others.size() > maxRows;
    std::size_t rows = std::min(others.size(), maxRows);

    float p = t.sideCardPadding, h = t.sideRowHeight;
    SideCard side;
    side.frame = { card.x + card.width + t.bubbleGap, card.y, t.sideCardWidth,
                   2 * p + static_cast<float>(rows) * h + static_cast<float>(rows - 1) * t.sideRowGap };
    side.rowRadius = std::max(0.0f, pillCornerRadius({ side.frame.width, side.frame.height }) - p);

    for (std::size_t i = 0; i < rows; ++i) {
        Rect row{ p, p + static_cast<float>(i) * (h + t.sideRowGap), side.frame.width - 2 * p, h };
        const Activity& a = *others[i];
        if (overflow && i + 1 == rows) {
            // The rest don't fit: one row stands for them and opens the first.
            std::string more = "+" + std::to_string(others.size() - rows + 1) + " more";
            side.elements.push_back({ .key = "side/more", .item = Text{ more, TextStyle::Caption }, .frame = row,
                                      .hitFrame = row, .activityId = a.id });
        } else {
            placeRow(a, row, side.rowRadius, side.elements);
        }
        side.rows.push_back({ row, a.id });
    }
    return side;
}

const SideRow* sideRowAt(const SideCard& side, Vec2 point)
{
    Vec2 local{ point.x - side.frame.x, point.y - side.frame.y };
    for (const SideRow& row : side.rows)
        if (row.frame.contains(local)) return &row;
    return nullptr;
}

} // namespace cup
