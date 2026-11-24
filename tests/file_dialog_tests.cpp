// A file dialog in front (decision 180): which window is one, which of its
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

    // Where the island hangs. A 1920x1080 monitor with a 48-high taskbar.
    const Rect work{ 0, 0, 1920, 1032 };
    const float pill = tokens().pickerPillRoom, card = tokens().pickerCardRoom;
    Rect dialog{ 500, 200, 900, 600 }; // its bottom at 800: 232 under it
    expect(hangUnder(dialog, work, false) == Hang{ 800, 950 }, "from the dialog's bottom edge, centred on it");
    expect(hangUnder(dialog, work, true) == Hang{ 1032 - card, 950 }, "a card that doesn't fit under it moves up over the dialog");
    Rect high{ 500, 0, 900, 400 };
    expect(hangUnder(high, work, true) == Hang{ 400, 950 }, "a card that fits stays under it");
    Rect low{ 500, 400, 900, 632 - pill + 1 }; // a pixel short of the room the pill needs
    expect(!hangUnder(low, work, false), "no room for the pill under it: the island stays in its own place");
    low.height -= 1;
    expect(hangUnder(low, work, false).has_value(), "just enough room: it hangs");
    expect(!hangUnder({ 2100, 200, 900, 600 }, work, false), "a dialog on another monitor: its own place");
    expect(!hangUnder({ 500, -900, 900, 600 }, work, false), "a dialog on the monitor above: its own place");
    Rect tall{ 500, 0, 900, 900 };
    expect(hangUnder(tall, { 0, 0, 1920, 400 + pill }, true) == std::optional<Hang>{}, "a dialog past the work area's bottom: its own place");
    expect(hangUnder({ 500, 0, 900, 300 }, { 0, 0, 1920, 400 }, true) == Hang{ 0, 950 }, "a screen shorter than the card: never above its top");
}

} // namespace cup::test
