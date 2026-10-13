#include "core/daydream.h"

#include "core/lift.h"
#include "core/tokens.h"

namespace cup {

void holdHomeShortcut(IslandState& s, Ms now)
{
    if (s.hidden) return; // the shortcuts are off anyway (decision 032)
    // Always a new press (MOD_NOREPEAT): a release that fell between two
    // polls was missed, so this hold starts afresh.
    s.shortcutHeldAt = now;
    s.daydream = false;
}

void releaseHomeShortcut(IslandState& s)
{
    s.shortcutHeldAt.reset();
    s.daydream = false;
}

bool settleDaydream(IslandState& s, Ms now)
{
    if (s.hidden) {
        releaseHomeShortcut(s);
        return false;
    }
    auto due = daydreamDeadline(s);
    if (!due || now < *due) return false;
    s.daydream = true;
    releaseIsland(s); // a lift in progress lands: the pointer can't carry it now
    return true;
}

std::optional<Ms> daydreamDeadline(const IslandState& s)
{
    if (!s.shortcutHeldAt || s.daydream) return std::nullopt;
    return *s.shortcutHeldAt + tokens().daydreamHold;
}

bool ignoredWhileDaydreaming(const Event& e)
{
    return std::holds_alternative<PillClicked>(e) || std::holds_alternative<BubbleClicked>(e) ||
           std::holds_alternative<IslandPressed>(e) || std::holds_alternative<BubbleDragged>(e) ||
           std::holds_alternative<TileClicked>(e) || std::holds_alternative<HomeOpened>(e);
}

} // namespace cup
