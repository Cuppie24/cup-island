// Updates live in Settings (decision 152): no Update widget. An update waiting
// puts an accent dot on Settings' gear and on its Updates page; What's new
// opens the offer's release notes, or this version's; a failed install
// offers its installer to run by hand.

#include "expect.h"
#include "panel_host.h"
#include "providers/system/settings_provider.h"

namespace cup::test {
namespace {

namespace P = settings_action;

void press(SettingsProvider& s, const std::string& actionId) { s.onAction({ "system.settings", "", actionId, 0, "", kSettingsWindowId }); }

void whatsNew()
{
    PanelHost h;
    std::vector<std::string> opened;
    SettingsProvider s({ .version = "0.12.10", .checkForUpdates = [](auto) {}, .openPage = [&](const std::string& url) { opened.push_back(url); } });
    s.start(h);
    s.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    const Row* row = windowRow(h, "What's new");
    expect(row && detailOf(h, "What's new") == "In 0.12.10, on GitHub" && itemOf<Button>(row)->actionId == P::kWhatsNew &&
               pageOf(h, kUpdatesPage)->sections[0].rows.size() == 2,
           "what's new: a row on Updates, under Check for updates, for the running version");
    press(s, P::kWhatsNew);
    expect(opened.size() == 1 && opened[0] == "https://github.com/Cuppie24/cup-island/releases/tag/v0.12.10", "what's new: this version's release notes");

    s.updateStatusChanged(UpdateStatus{ .version = "0.12.11", .page = "https://github.com/Cuppie24/cup-island/releases/tag/v0.12.11",
                                        .download = "https://github.com/Cuppie24/cup-island/releases/download/v0.12.11/CupIsland-Setup-0.12.11.exe" });
    press(s, P::kWhatsNew);
    expect(detailOf(h, "What's new") == "In 0.12.11, on GitHub" && opened.back().ends_with("/v0.12.11"),
           "what's new: an update offered, its notes instead: what the widget's button did");
    expect(!windowRow(h, "Download installer"), "download: not while the update can still install itself");
    s.updateStatusChanged(UpdateStatus{ .version = "0.12.11", .stage = UpdateStage::Failed, .problem = "Couldn't download",
                                        .download = "https://github.com/Cuppie24/cup-island/releases/download/v0.12.11/CupIsland-Setup-0.12.11.exe" });
    press(s, P::kDownloadUpdate);
    expect(windowRow(h, "Download installer") && opened.back().ends_with("CupIsland-Setup-0.12.11.exe"),
           "download: a failed install offers its installer, opened in the browser to run by hand");
    s.stop();

    PanelHost dh;
    SettingsProvider dev({ .version = "0.12.10-3-gabc", .openPage = [](const std::string&) {} });
    dev.start(dh);
    dev.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(!windowRow(dh, "What's new"), "what's new: a dev build has no release of its own");
    PanelHost nh;
    SettingsProvider closed({ .version = "0.12.10" });
    closed.start(nh);
    closed.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(!windowRow(nh, "What's new"), "what's new: an app that can't open pages has no row");
}

void accentDot()
{
    PanelHost h;
    h.store[SettingsProvider::kSeenKey] = "seen1\n";
    for (const SettingItem& item : announcedSettings()) h.store[SettingsProvider::kSeenKey] += item.id + "\n";
    SettingsProvider s({ .version = "0.12.10", .checkForUpdates = [](auto) {}, .installUpdate = [] {} });
    s.start(h);
    expect(!h.published.back().content.badge && !h.published.back().content.badgeAccent, "dot: nothing waiting, none on the gear");
    s.updateStatusChanged(UpdateStatus{ .version = "0.12.11" });
    expect(h.published.back().content.badgeAccent, "dot: an update waiting, an accent dot on Settings' gear");
    s.onAction({ "system.settings", SettingsProvider::kPanelId, P::kOpenWindow, 0 });
    expect(pageOf(h, kUpdatesPage)->badgeAccent && !pageOf(h, kAppearancePage)->badgeAccent, "dot: and beside Updates in the sidebar");
    s.updateStatusChanged(UpdateStatus{ .version = "0.12.11", .stage = UpdateStage::Installing });
    expect(!h.published.back().content.badgeAccent && !pageOf(h, kUpdatesPage)->badgeAccent, "dot: installing, nothing to do: none");
    s.updateStatusChanged(UpdateStatus{ .version = "0.12.11", .stage = UpdateStage::Failed, .problem = "Couldn't download" });
    expect(h.published.back().content.badgeAccent, "dot: a failure brings it back: it still needs doing");
    s.updateStatusChanged(std::nullopt);
    expect(!h.published.back().content.badgeAccent, "dot: withdrawn, gone");
    s.stop();
}

} // namespace

void runWhatsNewTests()
{
    whatsNew();
    accentDot();
}

} // namespace cup::test
