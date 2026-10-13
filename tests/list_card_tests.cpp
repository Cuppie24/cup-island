// The List card (decisions 030, 037): Settings' layout. Narrower rows,
// roomier than an activity's card, text clear of a row's rounded end, and
// groups with headings that line up and stand apart.

#include "core/layout.h"
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
    s = reduce(s, ActivityStarted{ { .id = "demo.settings", .content = content, .providerId = "demo" } }, Ms{ 0 });
    return reduce(s, PillClicked{}, Ms{ 0 });
}

} // namespace

void runListCardTests()
{
    const Tokens& t = tokens();
    // A List card (decision 030): a Centred stack with wider sides; a list
    // item's text at an edge sits further in than its buttons.
    Content settings{ .card = CardLayout::List,
                      .expandedCenter = { Row{ .items = { Text{ "Theme" }, Button{ Glyph::ChevronRight, "open" } }, .listItem = true },
                                          Row{ .items = { Swatch{}, Text{ "Grape" } }, .listItem = true } } };
    Scene listCard = layout(expandedWith(settings), Ms{ 0 });
    const Placed* name = find(listCard, "expandedCenter/0/0");
    const Placed* chevron = find(listCard, "expandedCenter/0/1");
    const Placed* swatch = find(listCard, "expandedCenter/1/0");
    float listWidth = std::min(listCard.size.x - 2 * (t.expandedPadding + t.listCardInset), t.listCardWidth);
    float listSide = (listCard.size.x - listWidth) / 2;
    expect(listWidth < listCard.size.x - 2 * (t.expandedPadding + t.listCardInset), "the shipped listCardWidth narrows the rows");
    expect(name && name->listRow && name->listRow->x == listSide && name->listRow->width == listWidth,
           "a List card's rows: no wider than listCardWidth, centred");
    expect(name->frame.x == listSide + t.listItemTextInset, "text at a list item's edge clears its rounded end");
    expect(chevron && chevron->frame.x + chevron->frame.width == listCard.size.x - listSide - t.listItemInset, "a button there keeps the usual inset");
    expect(swatch && swatch->frame.x == listSide + t.listItemInset && swatch->frame.width == t.swatchSize, "a swatch: a square of swatchSize");
    expect(listCard.size.y >= t.listCardMinHeight && listCard.size.y > t.expandedSize.y,
           "a List card is roomier than an activity's: listCardMinHeight at least");
    expect(name->listRow->y >= t.expandedPadding + t.listCardVerticalInset, "its rows sit listCardVerticalInset further in from the top");

    // Groups (decision 037): a heading starting one has sectionGap above it,
    // and lines up with the list items' text without being one.
    Content grouped{ .card = CardLayout::List,
                     .expandedCenter = { Row{ .items = { Text{ .value = "Dark", .style = TextStyle::Heading } }, .inset = true },
                                         Row{ .items = { Text{ "Grape" }, Button{ Glyph::Checked, "a" } }, .listItem = true },
                                         Row{ .items = { Text{ .value = "Light", .style = TextStyle::Heading } }, .section = true, .inset = true },
                                         Row{ .items = { Text{ "Paper" }, Button{ Glyph::Unchecked, "b" } }, .listItem = true } } };
    Scene groups = layout(expandedWith(grouped), Ms{ 0 });
    const Placed* dark = find(groups, "expandedCenter/0/0");
    const Placed* grapeName = find(groups, "expandedCenter/1/0");
    const Placed* grapeBox = find(groups, "expandedCenter/1/1");
    const Placed* light = find(groups, "expandedCenter/2/0");
    expect(dark && grapeName && dark->frame.x == grapeName->frame.x && !dark->listRow, "a heading lines up with the names under it, nothing lit");
    float gapAbove = light->frame.y - (grapeBox->frame.y + grapeBox->frame.height);
    float gapBelow = find(groups, "expandedCenter/3/0")->frame.y - (light->frame.y + light->frame.height);
    expect(gapAbove >= t.sectionGap && gapAbove > gapBelow, "a second group's heading has sectionGap more above it");
}

} // namespace cup::test
