// Windows' own theme (decision 150): which theme a mode comes to, and
// Settings' Window theme page: Island | System | Manual picked in place, the
// themes listed only while it's Manual.

#include "expect.h"
#include "panel_host.h"
#include "core/tokens.h"
#include "providers/system/settings_provider.h"
#include "providers/system/window_theme.h"

namespace cup::test {
namespace {

namespace P = settings_action;

std::vector<Theme> someThemes()
{
    return { { .name = "Midnight" }, { .name = "Paper", .island = { 246, 246, 248 }, .light = true }, { .name = "Grape" }, { .name = "Linen", .light = true } };
}

void resolving()
{
    auto themes = someThemes();
    expect(!windowThemeFor(WindowThemeMode::Island, "Grape", true, themes), "window theme: as the island, no theme of its own: the island's tokens");
    expect(windowThemeFor(WindowThemeMode::System, "", false, themes) == "Midnight" && windowThemeFor(WindowThemeMode::System, "", true, themes) == "Paper",
           "window theme: as Windows, the first dark or light theme in tokens.json's order");
    std::vector<Theme> darkOnly{ { .name = "Midnight" } };
    expect(!windowThemeFor(WindowThemeMode::System, "", true, darkOnly), "window theme: Windows light with no light theme: the island's");
    expect(windowThemeFor(WindowThemeMode::Manual, "Grape", true, themes) == "Grape" && windowThemeFor(WindowThemeMode::Manual, "Gone", false, themes) == "Midnight",
           "window theme: Manual, the picked one; one tokens.json no longer has, the first");
    expect(!windowThemeFor(WindowThemeMode::Manual, "Grape", false, {}), "window theme: no themes at all: the island's");
    expect(parseWindowThemeMode("system") == WindowThemeMode::System && parseWindowThemeMode("manual") == WindowThemeMode::Manual &&
               parseWindowThemeMode("") == WindowThemeMode::Island && parseWindowThemeMode("odd") == WindowThemeMode::Island &&
               std::string(windowThemeText(WindowThemeMode::Manual)) == "manual",
           "window theme: kept as text; nothing saved (or anything unknown) is as the island");
}

// The page's Segments row, or nullptr.
const Segments* modeControl(const PanelHost& h)
{
    const Row* r = windowRow(h, "Colours");
    return r ? itemOf<Segments>(r) : nullptr;
}

bool listsThemes(const PanelHost& h)
{
    const WindowPage* page = pageOf(h, kAppearancePage);
    return page && page->sections.size() == 3 && page->sections[1].title == "Dark" && page->sections[2].title == "Light";
}

void settingsPage()
{
    Tokens saved = tokens();
    Tokens themed = saved;
    themed.themes = someThemes();
    setTokens(themed);
    PanelHost h;
    h.store[SettingsProvider::kThemeKey] = "Grape";
    h.store[SettingsProvider::kSeenKey] = "seen1\nwindowtheme\n";
    std::vector<std::pair<WindowThemeMode, std::string>> applied;
    SettingsProvider settings({ .applyTheme = [](const std::string&) {},
                                .applyWindowTheme = [&](WindowThemeMode mode, const std::string& manual) { applied.push_back({ mode, manual }); } });
    settings.start(h);
    expect(applied.size() == 1 && applied.back().first == WindowThemeMode::Island, "window theme: nothing saved, windows look like the island");
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    const Row* row = windowRow(h, "Window theme");
    expect(row && detailOf(h, "Window theme") == "Like the island" && itemOf<Button>(row)->actionId == P::kOpenWindowTheme,
           "window theme: a row on Appearance, under Theme, saying what it's set to");

    auto press = [&](const std::string& actionId) { settings.onAction({ "system.settings", "", actionId, 0, "", kSettingsWindowId }); };
    press(P::kOpenWindowTheme);
    const WindowPage* page = pageOf(h, kAppearancePage);
    expect(page->title == "Window theme" && page->backAction == P::kBack && modeControl(h) && modeControl(h)->picked == 0 &&
               modeControl(h)->options == std::vector<std::string>{ "Island", "System", "Manual" } && !listsThemes(h),
           "window theme: its page, Island | System | Manual picked in place; no themes listed while it's the island's");

    press(std::string(P::kPickWindowThemeMode) + "1");
    expect(applied.back().first == WindowThemeMode::System && modeControl(h)->picked == 1 && !listsThemes(h) &&
               h.store[SettingsProvider::kWindowThemeKey] == "system",
           "window theme: System applies and saves; still no themes listed");

    press(std::string(P::kPickWindowTheme) + "0");
    expect(applied.back().first == WindowThemeMode::System, "window theme: a theme can't be picked unless it's Manual");

    press(std::string(P::kPickWindowThemeMode) + "2");
    expect(applied.back() == std::pair{ WindowThemeMode::Manual, std::string("Grape") } && modeControl(h)->picked == 2 && listsThemes(h),
           "window theme: Manual lists the themes, starting at the island's: nothing changes until one is picked");
    press(std::string(P::kPickWindowTheme) + "1");
    expect(applied.back() == std::pair{ WindowThemeMode::Manual, std::string("Paper") } && h.store[SettingsProvider::kWindowThemeNameKey] == "Paper" &&
               windowRow(h, "Paper") && itemOf<Button>(windowRow(h, "Paper"))->glyph == Glyph::Checked,
           "window theme: a theme picked applies, saves, and is ticked; the page stays: the window is the preview");

    press(std::string(P::kPickWindowThemeMode) + "0");
    expect(applied.back().first == WindowThemeMode::Island && !listsThemes(h), "window theme: back to the island's, the list goes");
    press(P::kBack);
    expect(detailOf(h, "Window theme") == "Like the island", "window theme: back on Appearance, the row says so");

    settings.stop();
    h.store[SettingsProvider::kWindowThemeKey] = "manual";
    settings.start(h);
    expect(applied.back() == std::pair{ WindowThemeMode::Manual, std::string("Paper") }, "window theme: the saved mode and theme apply at start");
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(detailOf(h, "Window theme") == "Paper", "window theme: Manual, the row names its theme");

    SettingsProvider without({ .applyTheme = [](const std::string&) {} });
    PanelHost bare;
    without.start(bare);
    without.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(!windowRow(bare, "Window theme"), "window theme: an app that can't theme its windows has no row");
    setTokens(saved);
}

} // namespace

void runWindowThemeTests()
{
    resolving();
    settingsPage();
}

} // namespace cup::test
