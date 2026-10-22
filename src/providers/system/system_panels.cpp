#include "providers/system/system_panels.h"

namespace cup {

// The mode it's in, the way Settings words it; a click switches (the tap action).
Panel themePanel(bool dark)
{
    const char* mode = dark ? "Dark" : "Light";
    Content content{
        .label = std::string(mode) + " mode",
        // One line: the icon beside its word, like the media and timer widgets (decision 073).
        .tile = { row({ Icon{ dark ? Glyph::Moon : Glyph::Sun }, Text{ mode, TextStyle::Title } }) },
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
