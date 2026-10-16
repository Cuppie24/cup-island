#pragma once
// The interaction window as an OLE drop target (decision 061). Reads what a
// drag holds (Explorer's files, items with a disk path, a link, text) into
// a DragSummary, reads the files' sizes off the UI thread, and asks the app
// what each point means. Decides nothing; never answers with a move.

#include "core/drop.h"
#include "platform/win.h"

#include <functional>
#include <string>
#include <vector>

namespace cup::platform {

// Points are screen pixels; the app makes them its own.
struct DropCallbacks {
    std::function<void(const DragSummary& drag)> onEnter;
    std::function<void(const DragSummary& drag)> onSizes; // sizes read: the same drag, complete (UI thread)
    std::function<DropEffect(POINT screen)> onOver;       // what landing here would do
    std::function<void()> onLeave;
    std::function<DropEffect(POINT screen, const std::vector<DroppedItem>& items)> onDrop;
    std::function<void(std::function<void()>)> postToUi;  // back to the UI thread from the size reader
    std::wstring storeFolder;                             // free space is read on its disk
    std::size_t maxChecked = 200;                         // sizes read for this many items at most
};

// After OleInitialize, on the UI thread. False if Windows refused (logged).
bool registerDropTarget(HWND window, DropCallbacks callbacks);
void revokeDropTarget(HWND window);

} // namespace cup::platform
