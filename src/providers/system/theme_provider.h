#pragma once
// Theme toggle on Home (decision 009): switches Windows between light and
// dark mode, apps and system together, like Settings → Personalization →
// Colors → "Choose your mode". It writes the two Personalize values and
// tells running apps (WM_SETTINGCHANGE "ImmersiveColorSet"). It watches the
// key, so a change made in Settings shows on the tile too: event-driven,
// a one-shot registry notification re-armed after each change.
// Every decision about the tile is in ThemePresenter.

#include "core/provider.h"
#include "providers/system/system_panels.h"

#include <winrt/Windows.Foundation.h>

#include <functional>
#include <memory>

namespace cup {

class ThemeProvider : public Provider {
public:
    using PostToUi = std::function<void(std::function<void()>)>;
    explicit ThemeProvider(PostToUi postToUi) : post_(std::move(postToUi)) {}

    const char* id() const override { return "system.theme"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action& action) override; // the tile's tap: switch mode

private:
    struct Watch; // the registry key and its change event, shared with the watching coroutine

    void read();
    winrt::fire_and_forget watch(std::shared_ptr<Watch> watch);
    static winrt::fire_and_forget announce(); // WM_SETTINGCHANGE, off the UI thread: a hung app can't stall the island

    PostToUi post_;
    std::unique_ptr<ThemePresenter> presenter_;
    std::shared_ptr<Watch> watch_;
    std::optional<bool> dark_;
};

} // namespace cup
