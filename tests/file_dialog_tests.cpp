// A file dialog in front (decision 181): which window is one, which of its
// controls is the "File name" field, and where the island hangs under it.

#include "core/file_dialog.h"
#include "core/tokens.h"
#include "expect.h"

namespace cup::test {

void runFileDialogTests()
{
    // Which window is one.
    expect(isFileDialog("#32770", { "WorkerW", "ReBarWindow32", "DUIViewWndClassName", "DirectUIHWND" }), "today's Open / Save dialog");
    expect(isFileDialog("#32770", { "ComboBoxEx32", "ToolbarWindow32", "SHELLDLL_DefView", "SysListView32" }), "the old-style dialog");
    expect(!isFileDialog("#32770", { "Button", "Static" }), "a message box is a dialog, not a file dialog");
    expect(!isFileDialog("#32770", { "SysTreeView32", "Button" }), "Browse for Folder has no file view");
    expect(!isFileDialog("CabinetWClass", { "DUIViewWndClassName", "SHELLDLL_DefView" }), "an Explorer window has the view but isn't a dialog");
    expect(mayBecomeFileDialog("#32770", { "ComboBoxEx32", "ToolbarWindow32" }), "an old-style dialog before its view is built: looked at again");
    expect(!mayBecomeFileDialog("#32770", { "DUIViewWndClassName" }), "one already isn't looked at again");
    expect(!mayBecomeFileDialog("Chrome_WidgetWin_1", {}), "only dialogs are");

    // Its "File name" field.
    expect(isFileNameField({ "ComboBoxEx32", "ComboBox", "Edit" }), "Open's file name box");
    expect(isFileNameField({ "DUIViewWndClassName", "DirectUIHWND", "FloatNotifySink", "ComboBox", "Edit" }), "Save's");
    expect(isFileNameField({ "Edit" }), "the oldest dialog's plain Edit");
    expect(!isFileNameField({ "WorkerW", "ReBarWindow32", "Address Band Root", "msctls_progress32", "ComboBoxEx32", "ComboBox", "Edit" }),
           "the address bar being typed in isn't it");
    expect(!isFileNameField({ "WorkerW", "ReBarWindow32", "UniversalSearchBand", "Search Box", "Edit" }), "nor search");
    expect(!isFileNameField({ "ComboBoxEx32", "ComboBox" }), "only an Edit takes the text");
    expect(!isFileNameField({}), "nothing isn't");

    // Where its island hangs. A 1920x1080 monitor with a 48-high taskbar; a 600x500 canvas, a 28-high pill.
    const Rect work{ 0, 0, 1920, 1032 };
    const Vec2 canvas{ 600, 500 };
    const float pill = 28, card = tokens().pickerCardRoom;
    Rect dialog{ 500, 200, 900, 600 }; // its bottom at 800: 232 under it
    expect(placeByDialog(dialog, work, canvas, pill, false) == DialogPlace{ { 650, 800 }, false }, "from the dialog's bottom edge, centred on it");
    expect(placeByDialog({ 500.4f, 200, 901, 600.3f }, work, canvas, pill, false).origin == Vec2{ 651, 800 }, "...on whole pixels");
    expect(placeByDialog(dialog, work, canvas, pill, true) == DialogPlace{ { 650, 1032 - card }, false },
           "a card that doesn't fit under it moves up over the dialog");
    expect(placeByDialog({ 500, 0, 900, 400 }, work, canvas, pill, true) == DialogPlace{ { 650, 400 }, false }, "a card that fits stays under it");
    expect(placeByDialog({ 500, 404, 900, 600 }, work, canvas, pill, false) == DialogPlace{ { 650, 1004 }, false }, "just room under it: under it");
    expect(placeByDialog({ 500, 405, 900, 600 }, work, canvas, pill, false) == DialogPlace{ { 650, 377 }, true },
           "a pixel short: over the dialog's top edge");
    expect(placeByDialog({ 500, 400, 900, 900 }, work, canvas, pill, false).above, "a dialog going behind the taskbar: over its top");
    expect(placeByDialog({ 500, 405, 900, 600 }, work, canvas, pill, true) == DialogPlace{ { 650, 1032 - card }, false },
           "...and its card opens under, as any");
    expect(placeByDialog({ 500, 10, 900, 1022 }, work, canvas, pill, false) == DialogPlace{ { 650, 1032 }, false },
           "no room above either (a dialog as tall as the screen): under it, cut off");
    expect(placeByDialog({ -200, 200, 900, 600 }, work, canvas, pill, false).origin.x == 0, "a dialog half off the left edge: kept on the screen");
    expect(placeByDialog({ 1500, 200, 900, 600 }, work, canvas, pill, false).origin.x == 1320, "...and off the right");
    expect(placeByDialog({ 2100, 200, 900, 600 }, { 1920, 0, 1920, 1032 }, canvas, pill, false).origin.x == 2250, "on whichever monitor the dialog is");
    expect(placeByDialog(dialog, { 0, 0, 500, 1032 }, canvas, pill, false).origin.x == 0, "a screen narrower than the canvas: from its left edge");
    expect(placeByDialog({ 500, 0, 900, 300 }, { 0, 0, 1920, 400 }, canvas, pill, true).origin.y == 0, "a screen shorter than the card: never above its top");
}

} // namespace cup::test
