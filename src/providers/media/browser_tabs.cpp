#include "providers/media/browser_tabs.h"

#include "platform/log.h"
#include "providers/media/tab_match.h"

#include <UIAutomation.h>

namespace cup {
namespace {

using Element = winrt::com_ptr<IUIAutomationElement>;

// Chromium's tab strip sits about 10-15 levels below the window; the caps
// keep an unusual tree from stalling the search.
constexpr int kMaxDepth = 24;
constexpr std::size_t kMaxVisited = 4000;

struct Walk {
    IUIAutomationTreeWalker* walker = nullptr;
    std::size_t visited = 0;
    std::vector<Element> tabs;
};

void collectTabs(Walk& walk, IUIAutomationElement* element, int depth)
{
    if (depth > kMaxDepth || ++walk.visited > kMaxVisited) return;
    CONTROLTYPEID type = 0;
    if (FAILED(element->get_CurrentControlType(&type))) return;
    if (type == UIA_TabItemControlTypeId) {
        Element tab;
        tab.copy_from(element);
        walk.tabs.push_back(tab);
        return;
    }
    if (type == UIA_DocumentControlTypeId) return; // a web page: huge, and never holds the tab strip

    Element child;
    walk.walker->GetFirstChildElement(element, child.put());
    while (child) {
        collectTabs(walk, child.get(), depth + 1);
        Element next;
        walk.walker->GetNextSiblingElement(child.get(), next.put());
        child = std::move(next);
    }
}

std::string nameOf(IUIAutomationElement* element)
{
    BSTR name = nullptr;
    std::string out;
    if (SUCCEEDED(element->get_CurrentName(&name)) && name) out = winrt::to_string(std::wstring_view(name, SysStringLen(name)));
    SysFreeString(name);
    return out;
}

// Chromium and Firefox tabs support "select"; older trees only a default action.
bool selectTab(IUIAutomationElement* tab)
{
    winrt::com_ptr<IUIAutomationSelectionItemPattern> item;
    if (SUCCEEDED(tab->GetCurrentPatternAs(UIA_SelectionItemPatternId, IID_PPV_ARGS(item.put()))) && item)
        return SUCCEEDED(item->Select());
    winrt::com_ptr<IUIAutomationLegacyIAccessiblePattern> legacy;
    if (SUCCEEDED(tab->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, IID_PPV_ARGS(legacy.put()))) && legacy)
        return SUCCEEDED(legacy->DoDefaultAction());
    return false;
}

} // namespace

HWND selectMediaTab(const std::vector<HWND>& windows, const std::string& mediaTitle)
{
    winrt::com_ptr<IUIAutomation> uia;
    if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(uia.put())))) return nullptr;
    winrt::com_ptr<IUIAutomationTreeWalker> walker;
    if (FAILED(uia->get_ControlViewWalker(walker.put())) || !walker) return nullptr;

    // Every window's tabs in one list: the playing one can be in any of them.
    std::vector<Element> tabs;
    std::vector<HWND> owners;
    std::vector<std::string> names;
    for (HWND hwnd : windows) {
        Element root;
        if (FAILED(uia->ElementFromHandle(hwnd, root.put())) || !root) continue;
        Walk walk{ walker.get() };
        collectTabs(walk, root.get(), 0);
        for (auto& tab : walk.tabs) {
            names.push_back(nameOf(tab.get()));
            owners.push_back(hwnd);
            tabs.push_back(tab);
        }
    }

    auto match = findMediaTab(names, mediaTitle);
    if (!match) {
        platform::logLine("media: no tab matched among %zu", names.size());
        return nullptr;
    }
    // Even if selecting fails, the window holding the tab is the right one to raise.
    if (!selectTab(tabs[*match].get())) platform::logLine("media: found the tab, but couldn't select it");
    return owners[*match];
}

} // namespace cup
