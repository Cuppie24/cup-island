#include "core/file_dialog.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

bool has(const std::vector<std::string>& classes, const char* name)
{
    return std::find(classes.begin(), classes.end(), name) != classes.end();
}

constexpr const char* kDialogClass = "#32770";

} // namespace

bool isFileDialog(const std::string& windowClass, const std::vector<std::string>& inside)
{
    if (windowClass != kDialogClass) return false; // message boxes, Print and Font are dialogs too: the view tells
    // Explorer's view: in today's dialog (Vista on), and in the old-style one.
    return has(inside, "DUIViewWndClassName") || has(inside, "SHELLDLL_DefView");
}

bool mayBecomeFileDialog(const std::string& windowClass, const std::vector<std::string>& inside)
{
    return windowClass == kDialogClass && !isFileDialog(windowClass, inside);
}

bool isFileNameField(const std::vector<std::string>& chain)
{
    if (chain.empty() || chain.back() != "Edit") return false;
    // The address bar turns into an Edit when clicked, and search has one on
    // some builds: a path typed there isn't a file's name.
    for (const char* other : { "Address Band Root", "Breadcrumb Parent", "UniversalSearchBand", "Search Box" })
        if (has(chain, other)) return false;
    return true;
}

Hang hangUnder(Rect dialog, Rect workArea, bool cardOpen)
{
    float bottom = dialog.y + dialog.height, floor = workArea.y + workArea.height;
    float top = bottom;
    if (cardOpen) top = std::max(std::min(bottom, floor - tokens().pickerCardRoom), workArea.y);
    return { top, dialog.x + dialog.width / 2 };
}

Rect pillFrame(Hang hang, Vec2 size)
{
    return snapToPixels({ hang.centre - size.x / 2, hang.top, size.x, size.y });
}

std::string pickingCard(const IslandState& s)
{
    for (const Panel& p : s.panels)
        if (p.picksFiles && p.tapAction.empty() && hasCard(p.content)) return p.id;
    return {};
}

bool isPickingCard(const IslandState& s, const std::string& id)
{
    const Panel* p = findPanel(s, id);
    return p && p->picksFiles;
}

} // namespace cup
