#include "providers/update/update_provider.h"

#include "core/tokens.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <utility>
#include <vector>

namespace cup {
namespace {

std::optional<long long> parseMs(const std::string& text)
{
    long long value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end == text.data()) return std::nullopt;
    return value;
}

// The saved offer (kOfferKey): a field a line, in UpdateOffer's order.
std::string offerText(const UpdateOffer& o)
{
    return versionText(o.version) + "\n" + o.url + "\n" + o.sha256 + "\n" + o.fileName + "\n" + o.page + "\n" +
           (o.mandatory ? "mandatory\n" : "");
}

std::optional<UpdateOffer> offerFrom(const std::string& text)
{
    std::vector<std::string> lines;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        at = end + 1;
    }
    if (lines.size() < 4) return std::nullopt;
    auto version = parseVersion(lines[0]);
    if (!version || lines[1].empty() || lines[2].empty() || lines[3].empty()) return std::nullopt;
    return UpdateOffer{ .version = *version, .url = lines[1], .sha256 = lines[2], .fileName = lines[3], .page = lines.size() > 4 ? lines[4] : "",
                        .mandatory = lines.size() > 5 && lines[5] == "mandatory" };
}

} // namespace

void UpdateProvider::start(Host& host)
{
    host_ = &host;
    running_ = parseVersion(hooks_.version);
    if (!running_ || !hooks_.fetchLatest) return; // a dev build: it never checks
    restore();

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
    confirming_ = false;
    announce_ = false;
    replies_.clear(); // nobody to tell any more
    host_->wakeAt(std::nullopt);
    withdraw(false); // off screen, but still saved: the app is closing, the offer stands
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
    bool confirming = std::exchange(confirming_, false); // install() asked: this answer says what to install
    const Tokens& t = tokens();
    if (!release) { // offline, or GitHub didn't answer: try again sooner than a day
        host_->wakeAt(host_->now() + t.updateRetry);
        answer(CheckOutcome::Failed, "");
        // Couldn't ask (offline, rate-limited): the offer as found is still a
        // release, and its download may well work (decision 172).
        if (confirming) download();
        return;
    }
    host_->save(kLastCheckKey, std::to_string(host_->wallClock().count()));
    host_->wakeAt(host_->now() + t.updateCheckInterval);

    auto offer = offerFor(*running_, *release);
    if (!offer) {
        withdraw(); // withdrawn, or nothing to move to after all: an install about to start is dropped too
        answer(CheckOutcome::UpToDate, "");
        return;
    }
    if (stage_ == UpdateStage::Installing && !confirming) { // mid-download: leave it be
        answer(CheckOutcome::Available, versionText(offer_->version));
        return;
    }
    // About to install: whatever is latest now, higher or lower than the
    // offer it was clicked for (decisions 040, 172); it stays Installing.
    if (!confirming && (!offer_ || offer_->version != offer->version)) {
        stage_ = UpdateStage::Offered;
        problem_.clear();
    }
    offer_ = offer;
    host_->save(kOfferKey, offerText(*offer_)); // still offered after a restart
    publish(); // Settings' gear gets its dot: no alert, no widget (decision 152)
    answer(CheckOutcome::Available, versionText(offer_->version));
    if (confirming) download();
}

void UpdateProvider::install()
{
    if (!host_ || !offer_ || stage_ == UpdateStage::Installing || !hooks_.install) return;
    stage_ = UpdateStage::Installing;
    problem_.clear();
    publish();
    // The offer may be days old, and a release may have come out since the
    // check that found it, even since the last one: ask again and install
    // the latest (decision 172). A check already under way answers for this.
    confirming_ = true;
    check();
}

void UpdateProvider::download()
{
    if (!host_ || !offer_) return;
    if (std::exchange(announce_, false))
        host_->postAlert({ .activityId = kPanelId,
                           .content = { .label = "Updating",
                                        .alertLeading = { row({ Icon{ Glyph::Download } }) },
                                        .alertCenter = { row({ Text{ "Updating", TextStyle::Title } }),
                                                         row({ Text{ "Cup Island " + versionText(offer_->version) + ": a required update",
                                                                     TextStyle::Caption } }) } },
                           .coalesceKey = "update" });
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
    if (offer_) listener_(status());
    else listener_(std::nullopt);
}

UpdateStatus UpdateProvider::status() const
{
    return { versionText(offer_->version), stage_, problem_, offer_->page, offer_->url.empty() ? std::string(kLatestReleasePage) : offer_->url,
             offer_->mandatory };
}

void UpdateProvider::publish()
{
    if (!host_ || !offer_) return;
    if (listener_) listener_(status());
}

void UpdateProvider::restore()
{
    // Offered before a restart: back in Settings at once, without waiting a day
    // for the next check (which withdraws it if it's gone). Already installed
    // (this build is that version): forgotten. No second alert.
    auto saved = offerFrom(host_->load(kOfferKey));
    if (!saved) return;
    if (!movesTo(*running_, saved->version)) {
        host_->save(kOfferKey, "");
        host_->save(kMandatoryTriesKey, "");
        return;
    }
    offer_ = saved;
    stage_ = UpdateStage::Offered;
    publish();
    if (offer_->mandatory) installMandatory();
}

// A mandatory release (decision 041): installed at the start after it was
// found, with no click. The user still knows: an alert says it's updating, and
// Settings shows it installing. At most kMandatoryTries times per version:
// an installer that keeps failing falls back to Settings' Update and restart
// rather than a download on every start. Like a click, it installs the
// latest release, which may be a later one (decision 172); the alert names
// that one, so it waits for the check.
void UpdateProvider::installMandatory()
{
    std::string v = versionText(offer_->version);
    std::string tried = host_->load(kMandatoryTriesKey); // "<version> <tries>"
    int tries = 0;
    if (tried.rfind(v + " ", 0) == 0) tries = std::atoi(tried.c_str() + v.size() + 1);
    if (tries >= kMandatoryTries) return;
    host_->save(kMandatoryTriesKey, v + " " + std::to_string(tries + 1));
    announce_ = true;
    install();
}

void UpdateProvider::withdraw(bool forget)
{
    if (forget && host_) host_->save(kOfferKey, ""); // withdrawn: not back after a restart either
    announce_ = false; // nothing installs, so nothing to say
    if (!offer_) return;
    offer_.reset();
    stage_ = UpdateStage::Offered;
    if (listener_) listener_(std::nullopt);
}

} // namespace cup
