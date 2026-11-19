// Home's theme widget (decision 009): which Host calls each reading
// produces, and what the tile says. Settings: settings_tests.cpp.

#include "expect.h"
#include "panel_host.h"
#include "providers/system/system_panels.h"

namespace cup::test {

void runSystemTests()
{
    // Theme: a toggle tile whose tap switches the mode; republished on change only.
    PanelHost th;
    ThemePresenter theme(th);
    theme.show(std::nullopt);
    expect(th.published.empty() && th.removed.empty(), "can't tell the mode: no tile");
    theme.show(true);
    const Panel& dark = th.published.back();
    expect(dark.id == ThemePresenter::kPanelId && dark.tapAction == ThemePresenter::kToggle && !hasCard(dark.content),
           "a tap-action tile, no card");
    expect(leadGlyph(dark) == Glyph::Moon && textAt(dark, 0, 0) == "Dark" && dark.content.tile.size() == 1,
           "dark: the moon in the side square, its word on one line beside it (decision 073)");
    theme.show(true);
    expect(th.published.size() == 1, "same mode: no republish");
    theme.show(false);
    expect(th.published.size() == 2 && leadGlyph(th.published.back()) == Glyph::Sun && textAt(th.published.back(), 0, 0) == "Light",
           "light: the sun");

    // ...and as a command (decision 160): the tile's own tap, named for the mode it switches to.
    expect(th.commands.size() == 1 && th.commands[0].id == ThemePresenter::kToggle &&
               th.commands[0].activityId == ThemePresenter::kPanelId && th.commands[0].title == "Switch Windows to dark mode",
           "light: a command to switch to dark, sending the tile's action");
    theme.show(true);
    expect(th.commands.size() == 1 && th.commands[0].title == "Switch Windows to light mode" && th.commands[0].glyph == Glyph::Sun,
           "dark: to light");
    theme.show(std::nullopt);
    expect(th.commands.empty(), "can't tell the mode: no command, as no tile");
}

} // namespace cup::test
