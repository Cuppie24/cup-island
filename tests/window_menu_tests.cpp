// A window card's "…" menu and a window question's field (decision 142).

#include "core/dialog.h"
#include "core/window_fields.h"
#include "core/window_layout.h"
#include "core/window_menu.h"
#include "core/window_page.h"
#include "expect.h"

#include <string>

namespace cup::test {
namespace {

OpenWindow board()
{
    WindowContent c{ .title = "Board" };
    WindowPage page{ .id = "p", .title = "P", .titled = false, .cards = true };
    for (int g = 0; g < 2; ++g) {
        std::string gid = std::to_string(g);
        WindowSection s{ .title = "Group " + gid, .rows = { row({ Text{ "Row" } }) }, .id = "g" + gid };
        s.menu = { MenuItem{ .label = "Rename", .actionId = "rename." + gid }, MenuItem{ .label = "Off", .actionId = "off", .enabled = false } };
        page.sections.push_back(s);
    }
    c.pages = { page };
    return { .key = { "w", "board" }, .content = c, .page = "p" };
}

auto measure = [](const Text& x) { return 8.0f * static_cast<float>(x.value.size()); };

WindowScene laid(const OpenWindow& w) { return windowLayout(w, { 1400, 900 }, 0, Ms{ 0 }, measure); }

const Placed* buttonFor(const WindowScene& s, const std::string& action)
{
    for (const Placed& p : s.page)
        if (auto* b = std::get_if<Button>(&p.item); b && b->actionId == action) return &p;
    return nullptr;
}

void menuTests()
{
    OpenWindow w = board();
    WindowScene s = laid(w);
    const Placed* more = buttonFor(s, std::string(kWindowCardMenu) + "g1");
    expect(more && std::get<Button>(more->item).glyph == Glyph::More, "a card with a menu: a … in its title row");
    expect(cardMenuOf(std::string(kWindowCardMenu) + "g1") == std::optional<std::string>("g1") && !cardMenuOf("rename.1"),
           "its action names the card; a provider's doesn't");
    std::optional<Menu> menu = openCardMenu(w, "g1");
    expect(menu && menu->row == "g1" && menu->items.size() == 2 && !menu->lit && !openCardMenu(w, "nope"), "opened: the card's items");
    auto placed = placeWindowMenu(w, s, *menu, Ms{ 0 }, measure);
    Rect button{ more->frame.x + s.pane.x, more->frame.y + s.pane.y - s.scroll, more->frame.width, more->frame.height };
    expect(placed && placed->frame.y >= button.y + button.height && std::abs(placed->frame.x + placed->frame.width - (button.x + button.width)) < 0.5f,
           "laid out under its …, right edges together");
    auto rename = menuChoice(s, *menu, 0);
    expect(rename && rename->actionId == "rename.1" && rename->windowId == "board" && !menuChoice(s, *menu, 1) && !menuChoice(s, *menu, 5),
           "chosen: its action from the window; a dim one or none: nothing");
    OpenWindow changed = w;
    changed.content.pages[0].sections[1].menu.pop_back();
    expect(!placeWindowMenu(changed, laid(changed), *menu, Ms{ 0 }, measure), "its items changed: it closes");
    changed.content.pages[0].sections.pop_back();
    expect(!placeWindowMenu(changed, laid(changed), *menu, Ms{ 0 }, measure), "its card gone: it closes");
}

void fieldTests()
{
    OpenWindow w = board();
    w.dialog = Dialog{ .title = "Rename group", .confirmLabel = "Rename", .confirmAction = "rename.1", .windowId = "board", .field = "Group 1",
                       .fieldMaxLength = 40 };
    WindowScene s = laid(w);
    const Placed* field = nullptr;
    const Placed* confirm = nullptr;
    for (const Placed& p : s.dialog->elements) {
        if (auto* input = std::get_if<TextInput>(&p.item); input && input->actionId == kDialogField) field = &p;
        if (auto* b = std::get_if<Button>(&p.item); b && b->actionId == kDialogConfirm) confirm = &p;
    }
    expect(field && confirm && field->frame.y < confirm->frame.y && std::get<TextInput>(field->item).value == "Group 1",
           "a question's field: its text, over its buttons");
    WindowTyping typing;
    dialogTakesKeys(typing, s);
    expect(typing.draft && typing.draft->actionId == kDialogField && typing.draft->text == "Group 1" && typing.draft->anchor == 0 &&
               typing.draft->caret == 7 && typing.draft->maxLength == 40,
           "it takes the keys as it appears, all its text selected");
    typeText(typing, s, "Work");
    // A click on its confirm button takes the keys first; laid out again, it takes them back with what was typed.
    blurField(typing, s);
    WindowScene again = windowLayout(w, { 1400, 900 }, 0, Ms{ 0 }, measure, &typing);
    dialogTakesKeys(typing, again);
    expect(typing.draft && typing.draft->text == "Work", "the keys back: what was typed stays");
    expect(dialogFieldText(typing, again) == "Work" && !typing.draft && typing.kept.empty(), "answered: its text, and it's forgotten");
    // Left without the keys (a click on Rename), its text is still what's answered.
    dialogTakesKeys(typing, again);
    typeText(typing, again, "Home");
    blurField(typing, again);
    expect(dialogFieldText(typing, again) == "Home", "...or as it was left");
}

} // namespace

void runWindowMenuTests()
{
    menuTests();
    fieldTests();
}

} // namespace cup::test
