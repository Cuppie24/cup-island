#include "providers/update/update_provider.h"

#include "core/tokens.h"

#include <algorithm>
#include <charconv>
#include <utility>

namespace cup {
namespace {

std::optional<long long> parseMs(const std::string& text)
{
    long long value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end == text.data()) return std::nullopt;
    return value;
}

Alert offerAlert(const UpdateOffer& offer)
{
    std::string v = versionText(offer.version);
    return { .activityId = UpdateProvider::kPanelId, // a click opens the update's card (core: onClick)
             .content = { .label = "Update available",
                          .alertLeading = { row({ Icon{ Glyph::Download } }) },
                          .alertCenter = { row({ Text{ "Update available", TextStyle::Title } }),
                                           row({ Text{ "Cup Island " + v + ": click to install", TextStyle::Caption } }) } },
             .coalesceKey = "update" };
}

} // namespace

Panel updatePanel(const UpdateOffer& offer, const std::string& running, UpdateStage stage, const std::string& problem)
{
    namespace A = update_action;
    std::string v = versionText(offer.version);
    std::string caption = stage == UpdateStage::Installing ? "Downloading and installing…"
                          : stage == UpdateStage::Failed   ? problem
                                                           : "You have " + running;
    Button install{ .actionId = A::kInstall, .label = stage == UpdateStage::Failed ? "Try again" : "Install and restart" };
    if (stage == UpdateStage::Installing) {
        install.label = "Installing…";
        install.enabled = false;
    }
    Row buttons = row({ install }, Align::Center);
    if (!offer.page.empty()) buttons.items.push_back(Button{ .actionId = A::kWhatsNew, .label = "What's new" });
    buttons.loose = true;
    Content content{
        .label = "Update to " + v,
        .card = CardLayout::Centred,
        .expandedCenter = { row({ Text{ .value = "Cup Island " + v, .style = TextStyle::Title, .centred = true } }),
                            row({ Text{ .value = caption, .style = TextStyle::Caption, .centred = true } }),
                            buttons },
        .tileLeading = { row({ Icon{ Glyph::Download } }, Align::Center) },
        .tile = { row({ Text{ "Update", TextStyle::Title } }), row({ Text{ v, TextStyle::Caption } }) },
    };
    return { .id = UpdateProvider::kPanelId, .content = content };
}

void UpdateProvider::start(Host& host)
{
    host_ = &host;
    running_ = parseVersion(hooks_.version);
    if (!running_ || !hooks_.fetchLatest) return; // a dev build: it never checks

    // Not during startup; and no sooner than a day after the last check,
    // restarts included. A clock that went back counts as due.
    const Tokens& t = tokens();
    Ms wait = t.updateFirstCheckDelay;
    if (auto last = parseMs(host_->load(kLastCheckKey))) {
        Ms since = host_->wallClock() - Ms{ *last };
        if (since >= Ms{ 0 } && since < t.updateCheckInterval) wait = std::max(wait, t.updateCheckInterval - since);
    }
    host_->wakeAt(host_->now() + wait);
}

void UpdateProvider::stop()
{
    if (!host_) return;
    *alive_ = false;
    alive_ = std::make_shared<bool>(true);
    checking_ = false;
    replies_.clear(); // nobody to tell any more
    host_->wakeAt(std::nullopt);
    withdraw();
    stage_ = UpdateStage::Offered;
    host_ = nullptr;
}

void UpdateProvider::onWake()
{
    if (host_) check();
}

void UpdateProvider::checkNow(Reply reply)
{
    if (!host_) return;
    if (!running_ || !hooks_.fetchLatest) {
        reply(CheckOutcome::DevBuild, "");
        return;
    }
    replies_.push_back(std::move(reply));
    check(); // no-op while one is under way: its answer goes to this reply too
}

void UpdateProvider::answer(CheckOutcome outcome, const std::string& version)
{
    std::vector<Reply> waiting = std::exchange(replies_, {});
    for (const Reply& reply : waiting) reply(outcome, version);
}

void UpdateProvider::check()
{
    if (checking_ || !running_) return;
    checking_ = true;
    std::shared_ptr<bool> alive = alive_;
    hooks_.fetchLatest([this, alive](std::optional<GitHubRelease> release) {
        if (*alive) checked(release);
    });
}

void UpdateProvider::checked(const std::optional<GitHubRelease>& release)
{
    checking_ = false;
    const Tokens& t = tokens();
    if (!release) { // offline, or GitHub didn't answer: try again sooner than a day
        host_->wakeAt(host_->now() + t.updateRetry);
        answer(CheckOutcome::Failed, "");
        return;
    }
    host_->save(kLastCheckKey, std::to_string(host_->wallClock().count()));
    host_->wakeAt(host_->now() + t.updateCheckInterval);

    auto offer = offerFor(*running_, *release);
    if (!offer) {
        withdraw(); // withdrawn, or nothing newer after all
        answer(CheckOutcome::UpToDate, "");
        return;
    }
    std::string v = versionText(offer->version);
    if (stage_ == UpdateStage::Installing) { // mid-download: leave it be
        answer(CheckOutcome::Available, versionText(offer_->version));
        return;
    }
    if (!offer_ || offer_->version != offer->version) {
        stage_ = UpdateStage::Offered;
        problem_.clear();
    }
    offer_ = offer;
    publish();
    if (host_->load(kAnnouncedKey) != v) { // once per version: a daily check doesn't nag
        host_->postAlert(offerAlert(*offer));
        host_->save(kAnnouncedKey, v);
    }
    answer(CheckOutcome::Available, v);
}

void UpdateProvider::onAction(const Action& action)
{
    if (!host_ || !offer_ || action.activityId != kPanelId) return;
    if (action.actionId == update_action::kWhatsNew) {
        if (hooks_.openPage && !offer_->page.empty()) hooks_.openPage(offer_->page);
    } else if (action.actionId == update_action::kInstall) {
        install();
    }
}

void UpdateProvider::install()
{
    if (!host_ || !offer_ || stage_ == UpdateStage::Installing || !hooks_.install) return;
    stage_ = UpdateStage::Installing;
    problem_.clear();
    publish();
    std::shared_ptr<bool> alive = alive_;
    hooks_.install(*offer_, [this, alive](std::string problem) {
        if (!*alive || !host_) return;
        stage_ = UpdateStage::Failed;
        problem_ = std::move(problem);
        publish();
    });
}

void UpdateProvider::onStatus(std::function<void(const std::optional<UpdateStatus>&)> listener)
{
    listener_ = std::move(listener);
    if (!listener_) return;
    if (offer_) listener_(UpdateStatus{ versionText(offer_->version), stage_, problem_ });
    else listener_(std::nullopt);
}

void UpdateProvider::publish()
{
    if (!host_ || !offer_) return;
    host_->publishPanel(updatePanel(*offer_, versionText(*running_), stage_, problem_));
    if (listener_) listener_(UpdateStatus{ versionText(offer_->version), stage_, problem_ });
}

void UpdateProvider::withdraw()
{
    if (!offer_) return;
    host_->removePanel(kPanelId);
    offer_.reset();
    stage_ = UpdateStage::Offered;
    if (listener_) listener_(std::nullopt);
}

} // namespace cup
