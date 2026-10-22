#include "providers/pomodoro/pomodoro_provider.h"

#include "core/tokens.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace cup {
namespace {

using Phase = PomodoroProvider::Phase;
namespace act = pomodoro_action;

std::string minutes(Ms duration) { return std::to_string(duration.count() / 60000) + " min"; }

Alert phaseAlert(const std::string& title, const std::string& detail, const std::string& activityId, Color accent)
{
    Content content{ .label = title,
                     .accent = accent,
                     .alertLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
                     .alertCenter = { row({ Text{ title, TextStyle::Title } }), row({ Text{ detail } }) } };
    // Time-sensitive: the user asked to be told (PS §6.1). Coalesced: a skip right after never stacks two.
    return { .activityId = activityId, .level = Level::TimeSensitive, .content = content, .coalesceKey = "pomodoro" };
}

bool startsWith(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }

} // namespace

void PomodoroProvider::start(Host& host)
{
    host_ = &host;
    todos_ = TodoList::load(host_->load(kTodosKey));
    plan_ = loadPlan(host_->load(kPlanKey));
    stats_ = loadStats(host_->load(kStatsKey));
    daysOff_ = loadDaysOff(host_->load(kDaysOffKey));
    page_ = PomodoroPage::Timer;
    showDormant();
    // A session from before a restart is back, paused where it was (decision 061).
    session_ = resumeSession(host_->load(kSessionKey), host_->now());
    if (session_) publish();
}

void PomodoroProvider::stop()
{
    if (!host_) return;
    // The app is quitting: kept where it is now, to come back paused (decision 061).
    if (session_) host_->save(kSessionKey, saveSession(*session_, host_->now()));
    if (session_) host_->endActivity(kPanelId);
    session_.reset();
    host_->removePanel(kPanelId);
    host_ = nullptr;
}

// Ctrl+W on the running card (decision 034): the stop button's question,
// so a stray key can't lose the session either.
void PomodoroProvider::onCloseRequested(const std::string& activityId)
{
    if (activityId == kPanelId) onAction({ id(), kPanelId, act::kStop });
}

// Closed on one of its pages: it opens on the timer next time, like Settings on its list.
void PomodoroProvider::onCardClosed(const std::string& activityId)
{
    if (!host_ || activityId != kPanelId || page_ == PomodoroPage::Timer) return;
    page_ = PomodoroPage::Timer;
    showCards();
}

// Shown: "today" may be another day since the dormant card was built (decision 061).
void PomodoroProvider::onCardShown(const std::string& activityId)
{
    if (!host_ || activityId != kPanelId || session_) return;
    if (glanceOf(stats_, daysOff_, localDay(host_->wallClock(), host_->utcOffset())) != shownGlance_) showDormant();
}

void PomodoroProvider::onAction(const Action& action)
{
    if (!host_ || action.activityId != kPanelId) return;
    namespace todo = todo_action;
    if (action.actionId == todo::kClearDone || action.actionId == todo::kClearAll) {
        host_->ask(clearQuestion(action.actionId, todos_, kPanelId)); // clears only once confirmed (decision 020)
        return;
    }
    if (todos_.apply(action.actionId, action.text, action.value)) {
        if (startsWith(action.actionId, todo::kToggle)) countTodo(action.actionId);
        host_->save(kTodosKey, todos_.save());
        showCards();
        if (action.actionId == todo::kAdd) { // the newest open one: keep it in view (decision 029)
            auto open = std::count_if(todos_.items().begin(), todos_.items().end(), [](const Todo& t) { return !t.done; });
            if (open > 0) host_->reveal(kPanelId, todo::kList, static_cast<std::size_t>(open - 1));
        }
        return;
    }
    // Its pages (decision 061): statistics, settings, and back to the timer.
    if (action.actionId == act::kOpenStats || action.actionId == act::kOpenSettings || action.actionId == act::kBack) {
        page_ = action.actionId == act::kOpenStats ? PomodoroPage::Stats : action.actionId == act::kOpenSettings ? PomodoroPage::Settings
                                                                                                                : PomodoroPage::Timer;
        showCards();
        return;
    }
    // Reset statistics (decision 071): asks first; confirmed, every day but today goes.
    if (action.actionId == act::kResetStats) {
        host_->ask(resetQuestion());
        return;
    }
    if (action.actionId == act::kResetStatsConfirmed) {
        long long today = localDay(host_->wallClock(), host_->utcOffset());
        std::erase_if(stats_.days, [&](const PomodoroDay& d) { return d.day != today; });
        saveStats();
        showCards();
        return;
    }
    if (action.actionId == act::kFakeStats && debug_) { // the next made-up history, then the real one again
        fake_ = (fake_ + 1) % 3;
        stats_ = fake_ ? fakeStats(localDay(host_->wallClock(), host_->utcOffset()), fake_) : loadStats(host_->load(kStatsKey));
        showCards();
        return;
    }
    if (startsWith(action.actionId, act::kToggleDayOff)) {
        std::size_t day = static_cast<std::size_t>(std::atoi(action.actionId.c_str() + std::char_traits<char>::length(act::kToggleDayOff)));
        if (day >= daysOff_.size()) return;
        daysOff_[day] = !daysOff_[day];
        host_->save(kDaysOffKey, saveDaysOff(daysOff_));
        showCards();
        return;
    }
    // The plan, while dormant (decision 044): kept, and shown at once.
    bool focusStep = action.actionId == act::kFocusLess || action.actionId == act::kFocusMore;
    if (!session_ && (focusStep || action.actionId == act::kBreakLess || action.actionId == act::kBreakMore)) {
        bool more = action.actionId == act::kFocusMore || action.actionId == act::kBreakMore;
        plan_ = adjustPlan(plan_, focusStep, more ? 1 : -1);
        host_->save(kPlanKey, savePlan(plan_));
        showDormant();
        return;
    }
    Ms now = host_->now();
    if (action.actionId == act::kStart && !session_) {
        ++today().started;
        saveStats();
        begin(Phase::Focus, 1);
    } else if (action.actionId == act::kPause && session_) {
        Timeline& tl = session_->timeline;
        tl.position = tl.positionAt(now);
        tl.anchoredAt = now;
        tl.playing = !tl.playing;
        publish();
    } else if (action.actionId == act::kSkip && session_) {
        advance(/* skipped */ true);
    } else if (action.actionId == act::kStop && session_) {
        host_->ask(stopQuestion(*session_)); // a misclick shouldn't lose the session (decision 020)
    } else if (action.actionId == act::kStopConfirmed && session_) {
        PomodoroDay& day = today(); // canceled; the focus spent still counts
        if (session_->phase == Phase::Focus) day.focus += session_->timeline.positionAt(now);
        ++day.canceled;
        saveStats();
        finish();
    }
}

void PomodoroProvider::onWake()
{
    if (!host_ || !session_ || !session_->timeline.playing) return;
    if (session_->timeline.positionAt(host_->now()) < session_->timeline.duration) {
        publish(); // woken early: ask again for the real end
        return;
    }
    advance(/* skipped */ false);
}

void PomodoroProvider::begin(Phase phase, std::size_t round)
{
    Ms duration = phaseDuration(phase, plan_);
    session_ = Session{ phase, round, { .duration = duration, .anchoredAt = host_->now(), .playing = true } };
    publish();
}

// The next phase (followingPhase), or the end after the long break. Each
// phase's end goes into today's statistics (decision 061).
void PomodoroProvider::advance(bool skipped)
{
    const Tokens& t = tokens();
    PomodoroDay& day = today();
    if (session_->phase == Phase::Focus) {
        day.focus += session_->timeline.positionAt(host_->now());
        if (skipped) ++session_->focusSkipped;
        ++(skipped ? day.focusSkipped : day.focusDone);
    } else if (skipped) {
        ++day.breaksSkipped;
    }
    std::optional<Session> next = followingPhase(*session_, plan_, host_->now());
    if (!next) {
        // Finished: all the phases, with few enough focus phases skipped.
        bool counts = sessionCounts(session_->focusSkipped);
        if (counts) ++day.finished;
        std::string detail = counts ? std::to_string(day.finished) + " finished today" : std::to_string(t.pomodoroRounds) + " rounds of focus";
        saveStats();
        finish();
        if (!skipped) host_->postAlert(phaseAlert("Session done", detail, "", accentOf(Phase::Break)));
        return;
    }
    saveStats();
    session_ = next;
    publish();
    if (skipped) return;
    if (next->phase == Phase::Focus)
        host_->postAlert(phaseAlert("Back to focus", "Round " + std::to_string(next->round) + " of " + std::to_string(t.pomodoroRounds),
                                    kPanelId, accentOf(Phase::Focus)));
    else
        host_->postAlert(phaseAlert(next->phase == Phase::LongBreak ? "Long break" : "Break time", minutes(phaseDuration(next->phase, plan_)),
                                    kPanelId, accentOf(next->phase)));
}

void PomodoroProvider::finish()
{
    session_.reset();
    host_->save(kSessionKey, "");
    host_->wakeAt(std::nullopt);
    showDormant(); // to-dos changed while it ran, and today's statistics, show on the dormant card too
    host_->endActivity(kPanelId);
}

void PomodoroProvider::showDormant()
{
    shownGlance_ = glanceOf(stats_, daysOff_, localDay(host_->wallClock(), host_->utcOffset()));
    Panel panel = pomodoroPanel(todos_, plan_, shownGlance_);
    if (!session_) panel.content = onPage(std::move(panel.content));
    host_->publishPanel(std::move(panel));
}

Content PomodoroProvider::onPage(Content card) const
{
    if (page_ == PomodoroPage::Settings) return withSettingsPage(std::move(card), plan_, session_.has_value(), daysOff_, fakeRow());
    if (page_ == PomodoroPage::Stats) return withStatsPage(std::move(card), glanceOf(stats_, daysOff_, localDay(host_->wallClock(), host_->utcOffset())));
    return card;
}

std::optional<int> PomodoroProvider::fakeRow() const { return debug_ ? std::optional<int>(fake_) : std::nullopt; }

void PomodoroProvider::showCards()
{
    if (session_) publish();
    else showDormant();
}

void PomodoroProvider::countTodo(const std::string& actionId)
{
    std::uint64_t id = std::strtoull(actionId.c_str() + std::char_traits<char>::length(todo_action::kToggle), nullptr, 10);
    auto it = std::find_if(todos_.items().begin(), todos_.items().end(), [&](const Todo& t) { return t.id == id; });
    if (it == todos_.items().end()) return;
    int& done = today().todosDone;
    done = it->done ? done + 1 : std::max(0, done - 1); // unticked: taken back, never below none
    saveStats();
}

PomodoroDay& PomodoroProvider::today() { return dayOf(stats_, localDay(host_->wallClock(), host_->utcOffset())); }

void PomodoroProvider::saveStats()
{
    if (fake_ == 0) host_->save(kStatsKey, cup::saveStats(stats_)); // a made-up history never replaces the real one
}

void PomodoroProvider::publish()
{
    const Timeline& tl = session_->timeline;
    host_->save(kSessionKey, saveSession(*session_, host_->now()));
    Content content = pomodoroContent(*session_, todos_, plan_);
    content = onPage(std::move(content));
    host_->startActivity({ .id = kPanelId, .relevance = 0.4f, .content = std::move(content) });
    host_->wakeAt(tl.playing ? std::optional<Ms>(tl.anchoredAt + (tl.duration - tl.position)) : std::nullopt);
}

} // namespace cup
