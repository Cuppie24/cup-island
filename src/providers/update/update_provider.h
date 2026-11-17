#pragma once
// Updates from GitHub Releases (decision 036). A release build asks GitHub
// once a day (Host::wakeAt; the last check is kept in the store, so restarts
// don't add checks). A newer release is offered in Settings, with a dot in
// the accent on its gear (decision 152; no widget, card or alert of its own):
// Update and restart downloads the installer, checks its SHA-256 and runs it
// silently, which closes the island, replaces it and starts it again. Nothing
// installs without a click (TR §10) but a mandatory release (decision 041).
// Dev builds never check. Fetching, downloading and running are the app's
// (UpdateHooks), so this stays pure and is tested on Linux.

#include "core/provider.h"
#include "providers/update/release.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cup {

struct UpdateHooks {
    std::string version; // the running build's (cup_version.h): only X.Y.Z checks
    // Asks GitHub for the latest release, then calls `done` on the UI thread:
    // the release, or nothing on any failure (offline, rate-limited, bad reply).
    std::function<void(std::function<void(std::optional<GitHubRelease>)> done)> fetchLatest;
    // Downloads the offer's installer, checks it and starts it; the app then
    // quits. If that fails, calls `failed` on the UI thread with what to tell the user.
    std::function<void(const UpdateOffer&, std::function<void(std::string)> failed)> install;
};

enum class UpdateStage { Offered, Installing, Failed };

// An offer and how far it got, for Settings' "Update and restart" row (decision 037).
struct UpdateStatus {
    std::string version; // the offer's
    UpdateStage stage = UpdateStage::Offered;
    std::string problem; // Failed: why
    std::string page;    // its release's page, what's new in it; empty: none
    std::string download; // its installer, to download and run by hand when installing fails
    bool mandatory = false; // installs by itself at the next start (decision 041)
    bool operator==(const UpdateStatus&) const = default;
};

class UpdateProvider : public Provider {
public:
    explicit UpdateProvider(UpdateHooks hooks) : hooks_(std::move(hooks)) {}
    ~UpdateProvider() override { *alive_ = false; } // a check or download still running calls back into nothing

    const char* id() const override { return "system.update"; }
    void start(Host& host) override; // schedules the first check (none for a dev build)
    void stop() override;
    void onAction(const Action&) override {} // nothing of its own on screen: Settings has the buttons
    void onWake() override;                       // time for a check

    // "Check for updates" (Settings): checks now, then tells `reply` what it
    // found (Available: `version` is the offer's). A check already under way
    // answers it too. An offer is shown as after any check.
    using Reply = std::function<void(CheckOutcome, const std::string& version)>;
    void checkNow(Reply reply);

    // Installs the offer: Settings' Update and restart (decisions 037, 152).
    // Nothing without an offer, or while installing.
    void install();

    // Told whenever the offer or its stage changes: nothing when there's no
    // offer (none yet, withdrawn, or stopped). Settings shows it.
    void onStatus(std::function<void(const std::optional<UpdateStatus>&)> listener);

    static constexpr const char* kPanelId = "system.update"; // its alerts' (a mandatory release installing)
    // Store keys: the wall-clock time (ms) of the last check that got an answer.
    static constexpr const char* kLastCheckKey = "last-check";
    // The offer, so it's still in Settings after a restart (decision 039): its
    // version, installer link, checksum, file name and page, a line each.
    static constexpr const char* kOfferKey = "offer";
    // Mandatory releases (decision 041): installed by itself at the next
    // start, at most kMandatoryTries times per version ("<version> <tries>").
    static constexpr const char* kMandatoryTriesKey = "mandatory-tries";
    static constexpr int kMandatoryTries = 3;

private:
    void check();
    void checked(const std::optional<GitHubRelease>& release);
    void answer(CheckOutcome outcome, const std::string& version); // every waiting Reply
    void publish(); // the status, to whoever listens (Settings)
    UpdateStatus status() const; // the offer's, at its stage
    void withdraw(bool forget = true); // no offer any more; forget: and not after a restart either (not when just stopping)
    void restore();  // the saved offer, at start, if it's still another version than this build
    void installMandatory(); // a mandatory offer at start: installs it, up to kMandatoryTries times

    UpdateHooks hooks_;
    Host* host_ = nullptr;
    std::optional<Version> running_;
    std::optional<UpdateOffer> offer_;
    UpdateStage stage_ = UpdateStage::Offered;
    std::string problem_;
    bool checking_ = false;
    std::vector<Reply> replies_;
    std::function<void(const std::optional<UpdateStatus>&)> listener_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); // false once stopped: late callbacks do nothing
};

} // namespace cup
