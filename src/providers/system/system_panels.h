#pragma once
// Home's system widget (decision 009): the theme toggle. The presenter turns
// plain readings into a panel: no WinRT here, so every rule is tested on
// Linux; the Windows provider only reads and feeds readings in. It publishes
// only on change, and removes the panel when there's nothing to show.

#include "core/provider.h"

#include <optional>
#include <string>

namespace cup {

// Windows' light/dark mode (apps and system together), as a toggle tile.
class ThemePresenter {
public:
    explicit ThemePresenter(Host& host) : host_(host) {}
    void show(std::optional<bool> dark); // nullopt = can't tell (no such setting): no tile
    static constexpr const char* kPanelId = "system.theme";
    static constexpr const char* kToggle = "toggle"; // the tile's tap action

private:
    Host& host_;
    std::optional<bool> last_;
};

// Content, pure.
Panel themePanel(bool dark);

} // namespace cup
