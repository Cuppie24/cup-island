// Grouped rows (decision 090): a quiet label over rows on one background,
// each a name with a description under it and a control at its end.

#include "core/groups.h"
#include "core/hit_test.h"
#include "core/item_metrics.h"
#include "core/keycaps.h"
#include "core/layout.h"
#include "core/segments.h"
#include "core/tokens.h"
#include "expect.h"

namespace cup::test {
namespace {

const Placed* find(const Scene& scene, const std::string& key)
{
    for (const auto& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

IslandState expandedWith(const Content& content)
{
    IslandState s;
    s = reduce(s, ActivityStarted{ { .id = "demo.groups", .content = content, .providerId = "demo" } }, Ms{ 0 });
    return reduce(s, PillClicked{}, Ms{ 0 });
}

} // namespace

void runGroupTests()
{
    const Tokens& t = tokens();

    // The builders: a label, then rows marked grouped; a control at the end makes the whole row its button.
    Slot slot;
    addGroup(slot, "General", { groupItem(itemText("Start with Windows"), { Switch{ .actionId = "auto" } }),
                                groupItem(itemText("Copy logs"), { Button{ .glyph = Glyph::Copy, .actionId = "logs", .compact = true } }) });
    addGroup(slot, "Updates", { groupItem(itemText("Check for updates", "Version 0.6.14"), { Button{ Glyph::Download, "check" } }) });
    addGroup(slot, "Empty", {});
    addGroup(slot, "", { groupItem(itemText("A key", ""), { Text{ "Ctrl+Q" } }) });
    expect(slot.size() == 6 && std::get<Text>(slot[0].items[0]).style == TextStyle::Heading && !slot[0].section && !slot[0].grouped &&
           slot[3].section && slot[1].grouped && slot[1].clickAnywhere && slot[1].listItem && slot[2].grouped,
           "groups: a label (none above the first), its rows grouped and clickable across");
    expect(std::get<Text>(slot[4].items[0]).detail == "Version 0.6.14" && std::get<Text>(slot[4].items[0]).detailTrimsEnd,
           "groups: an item's description is its text's detail, cut at its end");
    expect(slot[5].grouped && slot[5].section && !slot[5].clickAnywhere && !slot[5].listItem,
           "groups: an empty group adds nothing; an unlabelled one is set apart by its first row; nothing to click, nothing lit");

    // Laid out: one background per row, meeting with no seam, rounded only at the group's ends.
    Content card{ .card = CardLayout::List, .expandedCenter = slot };
    Scene scene = layout(expandedWith(card), Ms{ 0 });
    const Placed* first = find(scene, "expandedCenter/1/group");
    const Placed* second = find(scene, "expandedCenter/2/group");
    const Placed* lone = find(scene, "expandedCenter/4/group");
    const Placed* label = find(scene, "expandedCenter/0/0");
    expect(first && second && lone && !find(scene, "expandedCenter/0/group"), "groups: each grouped row has its piece; a label has none");
    auto fill = [](const Placed* p) { return std::get<GroupFill>(p->item); };
    expect(fill(first).roundTop && !fill(first).roundBottom && !fill(second).roundTop && fill(second).roundBottom &&
           fill(lone).roundTop && fill(lone).roundBottom && first->cornerRadius == t.groupRadius,
           "groups: rounded at the group's ends only");
    expect(first->frame.y + first->frame.height == second->frame.y && first->frame.x == second->frame.x &&
           first->frame.width == second->frame.width,
           "groups: one row's piece ends where the next one's starts");
    expect(label && label->frame.x == first->frame.x && label->frame.y + label->frame.height <= first->frame.y,
           "groups: the label sits over its group, at its edge");
    const Placed* name = find(scene, "expandedCenter/1/0");
    const Placed* control = find(scene, "expandedCenter/1/1");
    expect(name && name->frame.x == first->frame.x + t.listItemTextInset && control &&
           control->frame.x + control->frame.width == first->frame.x + first->frame.width - t.listItemInset,
           "groups: the name and the control inset in the background, like a list item's");
    expect(name->listRow && name->listRow->y == second->frame.y - first->frame.height && name->listRow->height <= first->frame.height,
           "groups: a row lights within its own piece");
    const Placed* switchRow = find(scene, "expandedCenter/1/0"); // a lit row's band is its own height, without the gap below it
    const Placed* buttonRow = find(scene, "expandedCenter/2/0");
    const Placed* keyRow = find(scene, "expandedCenter/5/group"); // the last row: its piece is its height
    expect(switchRow->listRow && buttonRow->listRow && switchRow->listRow->height == buttonRow->listRow->height && keyRow &&
           std::abs(keyRow->frame.height - (t.smallButtonSize + 2 * t.groupItemPadding)) < 0.01f,
           "groups: a row ending in a switch, or in nothing, is as tall as one ending in a button");
    float tall = heightOf(slot[4].items[0], { 0, 0, 300, 300 }, 0) + 2 * t.groupItemPadding;
    expect(std::abs(lone->frame.height - tall) < 0.01f, "groups: a row with a description is two lines tall, plus groupItemPadding");
    auto hit = hitTest(scene, { first->frame.x + first->frame.width / 2, first->frame.y + 2 });
    expect(hit && hit->action.actionId == "auto" && first->hitFrame.width == 0, "groups: a background is never clicked: its row's control is");

    // Scrolled, a background keeps its exact size: only a real size change
    // rebuilds an element, and a rebuilt one fades in, so a size that moved by
    // a float's rounding made every piece flicker as the list scrolled (decision 071).
    {
        std::vector<Row> many;
        for (int i = 0; i < 12; ++i) many.push_back(groupItem(itemText("Row " + std::to_string(i), "under it"), { Button{ Glyph::ChevronRight, "go" } }));
        Slot scrolling;
        addGroup(scrolling, "Long", std::move(many), "long");
        Content longCard{ .card = CardLayout::List, .expandedCenter = scrolling };
        IslandState st = expandedWith(longCard);
        std::vector<float> was;
        for (const Placed& e : layout(st, Ms{ 0 }).elements)
            if (std::holds_alternative<GroupFill>(e.item)) was.push_back(e.frame.height);
        bool same = !was.empty();
        for (float offset : { 0.37f, 13.1f, 57.93f }) {
            st = reduce(st, ListScrolled{ "demo.groups", "long", 0, offset, 10000 }, Ms{ 0 });
            std::size_t i = 0;
            for (const Placed& e : layout(st, Ms{ 0 }).elements)
                if (std::holds_alternative<GroupFill>(e.item)) same = same && i < was.size() && e.frame.height == was[i++];
        }
        expect(same, "groups: scrolled, every background keeps its exact size: nothing rebuilt, nothing flickers");
    }

    // Keys (decision 092): a combination's keys in caps, the words between them plain.
    using KP = std::vector<KeyPart>;
    expect(keyParts("Ctrl+Shift+Tab") == KP{ { "Ctrl" }, { "Shift" }, { "Tab" } } &&
           keyParts("Enter or Space") == KP{ { "Enter" }, { "or", false }, { "Space" } } &&
           keyParts("1 to 9") == KP{ { "1" }, { "to", false }, { "9" } } &&
           keyParts("Double Ctrl") == KP{ { "Double", false }, { "Ctrl" } } && keyParts("Ctrl++") == KP{ { "Ctrl" }, { "+" } },
           "keys: split into caps and the words between them");
    MeasureText eight = [](const Text& text) { return 8.0f * static_cast<float>(text.value.size()); };
    float capped = keysWidth(Keys{ "Ctrl+Q" }, eight);
    expect(capped == (8 * 4 + 1 + 2 * t.keyCapPadding) + t.keyCapGap + t.keyCapHeight &&
           heightOf(Keys{ "Q" }, { 0, 0, 100, 100 }, 0) == t.keyCapHeight,
           "keys: caps as wide as their label plus padding, a one-letter one square; keyCapHeight tall");

    // Segments (decision 093): every option as wide as the widest, inside the track's inset.
    Segments speeds{ .options = { "Fast", "Medium", "Slow" }, .picked = 1, .actionId = "fade.pick:" };
    float option = 8 * 6 + 1 + 2 * t.segmentPadding;
    Rect at{ 100, 10, segmentsWidth(speeds, eight), t.segmentHeight };
    expect(at.width == 3 * option + 2 * t.segmentInset, "segments: the widest label sets every option's width");
    Rect slow = segmentFrame(speeds, at, 2);
    expect(slow.x == 100 + t.segmentInset + 2 * option && slow.width == option && slow.height == t.segmentHeight - 2 * t.segmentInset,
           "segments: each option in its share, inset from the track");
    expect(segmentAt(speeds, at, slow.x + 1) == 2u && segmentAt(speeds, at, 0) == 0u && segmentAt(speeds, at, 1000) == 2u &&
           segmentAction(speeds, 2) == "fade.pick:2",
           "segments: a point picks the option under it, the nearest past the ends; its action is the prefix and index");

    // A name keeps the room its description needs: the description isn't cut beside a short name.
    MeasureText perChar = [](const Text& text) { return 8.0f * static_cast<float>(text.value.size()); };
    Text named = itemText("Theme", "Midnight blue");
    expect(textWidth(named, perChar) == perChar(Text{ .value = "Midnight blue", .style = TextStyle::Caption }),
           "groups: a text is as wide as its wider line");
}

} // namespace cup::test
