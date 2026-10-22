#include "providers/system/system_panels.h"

namespace cup {

// The mode it's in, the way Settings words it; a click switches (the tap action).
Panel themePanel(bool dark)
{
    const char* mode = dark ? "Dark" : "Light";
    Content content{
        .label = std::string(mode) + " mode",
        // One line: the icon in the square at the side, like the media and timer widgets', so they line up (decision 073).
        .tileLeading = { row({ Icon{ dark ? Glyph::Moon : Glyph::Sun } }, Align::Center) },
        .tile = { row({ Text{ mode, TextStyle::Title } }) },
    };
    return { .id = ThemePresenter::kPanelId, .content = content, .tapAction = ThemePresenter::kToggle };
}

void ThemePresenter::show(std::optional<bool> dark)
{
    if (dark == last_) return;
    last_ = dark;
    if (dark) host_.publishPanel(themePanel(*dark));
    else host_.removePanel(kPanelId);
}

} // namespace cup
