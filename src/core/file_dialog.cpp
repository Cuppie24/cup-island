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

DialogPlace placeByDialog(Rect dialog, Rect workArea, Vec2 canvas, float pillHeight, bool cardOpen)
{
    float bottom = dialog.y + dialog.height, floor = workArea.y + workArea.height;
    float left = dialog.x + dialog.width / 2 - canvas.x / 2;
    float room = workArea.x + workArea.width - canvas.x; // the rightmost it may start
    left = room < workArea.x ? workArea.x : std::clamp(left, workArea.x, room);
    if (cardOpen) return { { snapEdge(left), snapEdge(std::max(std::min(bottom, floor - tokens().pickerCardRoom), workArea.y)) }, false };
    bool fitsUnder = bottom + pillHeight <= floor;
    bool fitsAbove = dialog.y - pillHeight >= workArea.y;
    bool above = !fitsUnder && fitsAbove;
    return { { snapEdge(left), snapEdge(above ? dialog.y - pillHeight : bottom) }, above };
}

} // namespace cup
