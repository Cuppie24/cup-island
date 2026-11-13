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
    if (windowOpen_) host_->closeWindow(kWindowId);
    windowOpen_ = false;
    host_->removePanel(kPanelId);
    host_ = nullptr;
}

// Ctrl+W on the running card (decision 034): the stop button's question,
// so a stray key can't lose the session either.
void PomodoroProvider::onCloseRequested(const std::string& activityId)
{
    if (activityId == kPanelId) onAction({ id(), kPanelId, act::kStop });
}

// Shown: "today" may be another day since the window was last filled (decision 061).
void PomodoroProvider::onCardShown(const std::string& activityId)
{
    if (host_ && activityId == kPanelId) showWindow();
}

// Closed: a question it was asking goes unanswered, an Undo with it.
void PomodoroProvider::onWindowClosed(const std::string& windowId)
{
    if (windowId != kWindowId) return;
    windowOpen_ = false;
    question_ = PomodoroQuestion::None;
    undoInWindow_ = false;
}

// The to-dos (decisions 016, 020, 071): on the card (a test fixture's) a
// clear asks with a dialog and a delete with a toast; in the window both are in place.
bool PomodoroProvider::todoAction(const Action& action, bool fromWindow)
{
    namespace todo = todo_action;
    const std::string& id = action.actionId;
    if (id == todo::kClearDone || id == todo::kClearAll) {
        if (fromWindow) {
            ask(id == todo::kClearDone ? PomodoroQuestion::ClearDone : PomodoroQuestion::ClearAll);
        } else {
            host_->ask(clearQuestion(id, todos_, kPanelId)); // clears only once confirmed (decision 020)
        }
        return true;
    }
    // Deleted at once, with an Undo (decision 071): the one deleted is kept until then.
    if (id == todo::kUndoDelete) {
        if (deleted_) todos_.restore(*deleted_);
        deleted_.reset();
        undoInWindow_ = false;
        host_->save(kTodosKey, todos_.save());
        showAll();
        return true;
    }
    bool deleting = startsWith(id, todo::kDelete);
    if (deleting) {
        std::uint64_t todoId = std::strtoull(id.c_str() + std::char_traits<char>::length(todo::kDelete), nullptr, 10);
        auto it = std::find_if(todos_.items().begin(), todos_.items().end(), [&](const Todo& t) { return t.id == todoId; });
        if (it != todos_.items().end()) {
            deleted_ = *it;
            undoInWindow_ = fromWindow;
            if (!fromWindow) host_->toast({ .activityId = kPanelId, .text = "To-do deleted", .actionLabel = "Undo", .actionId = todo::kUndoDelete });
        }
    }
    if (!todos_.apply(id, action.text, action.value)) return startsWith(id, "todo.");
    if (!deleting) undoInWindow_ = false; // the next change: the Undo row goes
    if (id == todo::kClearDoneConfirmed || id == todo::kClearAllConfirmed) question_ = PomodoroQuestion::None;
    if (startsWith(id, todo::kToggle)) countTodo(id);
    host_->save(kTodosKey, todos_.save());
    showAll();
    if (id == todo::kAdd && !fromWindow) { // the newest open one: keep it in view (decision 029)
        auto open = std::count_if(todos_.items().begin(), todos_.items().end(), [](const Todo& t) { return !t.done; });
        if (open > 0) host_->reveal(kPanelId, todo::kList, static_cast<std::size_t>(open - 1));
    }
    return true;
}

void PomodoroProvider::onAction(const Action& action)
{
    bool fromWindow = action.windowId == kWindowId;
    if (!host_ || (action.activityId != kPanelId && !fromWindow)) return;
    const std::string& id = action.actionId;
    if (id == act::kOpenWindow) return openWindow();
    if (id == kWindowPage) return ask(PomodoroQuestion::None); // another page: a question there goes unanswered
    if (id == act::kCancel) return ask(PomodoroQuestion::None);
    if (todoAction(action, fromWindow)) return;
    // Reset statistics (decision 071): asks first; confirmed, every day but today goes.
    if (id == act::kResetStats) {
        if (fromWindow) ask(PomodoroQuestion::ResetStats);
        else host_->ask(resetQuestion());
        return;
    }
    if (id == act::kResetStatsConfirmed) {
        long long day = localDay(host_->wallClock(), host_->utcOffset());
        std::erase_if(stats_.days, [&](const PomodoroDay& d) { return d.day != day; });
        question_ = PomodoroQuestion::None;
        saveStats();
        showAll();
        return;
    }
    if (id == act::kFakeStats && debug_) { // the next made-up history, then the real one again
        fake_ = (fake_ + 1) % 3;
        stats_ = fake_ ? fakeStats(localDay(host_->wallClock(), host_->utcOffset()), fake_) : loadStats(host_->load(kStatsKey));
        showAll();
        return;
    }
    if (startsWith(id, act::kToggleDayOff)) {
        std::size_t day = static_cast<std::size_t>(std::atoi(id.c_str() + std::char_traits<char>::length(act::kToggleDayOff)));
        if (day >= daysOff_.size()) return;
        daysOff_[day] = !daysOff_[day];
        host_->save(kDaysOffKey, saveDaysOff(daysOff_));
        showAll();
        return;
    }
    // The plan, while dormant (decision 044): kept, and shown at once.
    bool focusStep = id == act::kFocusLess || id == act::kFocusMore;
    if (!session_ && (focusStep || id == act::kBreakLess || id == act::kBreakMore)) {
        bool more = id == act::kFocusMore || id == act::kBreakMore;
        plan_ = adjustPlan(plan_, focusStep, more ? 1 : -1);
        host_->save(kPlanKey, savePlan(plan_));
        showAll();
        return;
    }
    Ms now = host_->now();
    if (id == act::kStart && !session_) {
        ++today().started;
        saveStats();
        begin(Phase::Focus, 1);
    } else if (id == act::kPause && session_) {
        Timeline& tl = session_->timeline;
        tl.position = tl.positionAt(now);
        tl.anchoredAt = now;
        tl.playing = !tl.playing;
        publish();
    } else if (id == act::kSkip && session_) {
        advance(/* skipped */ true);
    } else if (id == act::kStop && session_) {
        if (fromWindow) ask(PomodoroQuestion::Stop); // asked in place: the window has no dialogs
        else host_->ask(stopQuestion(*session_)); // a misclick shouldn't lose the session (decision 020)
    } else if (id == act::kStopConfirmed && session_) {
        PomodoroDay& day = today(); // canceled; the focus spent still counts
        if (session_->phase == Phase::Focus) day.focus += session_->timeline.positionAt(now);
        ++day.canceled;
        question_ = PomodoroQuestion::None;
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
    showDormant();
    host_->endActivity(kPanelId);
    showWindow(); // dormant there too, with today's statistics
}

void PomodoroProvider::showDormant() { host_->publishPanel(pomodoroPanel(plan_)); }

std::optional<int> PomodoroProvider::fakeRow() const { return debug_ ? std::optional<int>(fake_) : std::nullopt; }

void PomodoroProvider::showAll()
{
    if (session_) publish(); // the window too
    else {
        showDormant();
        showWindow();
    }
}

void PomodoroProvider::ask(PomodoroQuestion question)
{
    question_ = question;
    showWindow();
}

PomodoroGlance PomodoroProvider::glance() const { return glanceOf(stats_, daysOff_, localDay(host_->wallClock(), host_->utcOffset())); }

void PomodoroProvider::showWindow()
{
    if (!windowOpen_) return;
    host_->updateWindow(kWindowId, pomodoroWindow({ .session = session_, .todos = &todos_, .plan = plan_, .glance = glance(), .daysOff = daysOff_,
                                                    .question = question_, .undoDelete = undoInWindow_ && deleted_, .fake = fakeRow() }));
}

// The card's expand button (decision 106): the window opens on the timer, or
// comes to the front where the user left it, like Settings' (decision 104).
void PomodoroProvider::openWindow()
{
    PomodoroWindowView view{ .session = session_, .todos = &todos_, .plan = plan_, .glance = glance(), .daysOff = daysOff_, .fake = fakeRow() };
    bool wasOpen = windowOpen_;
    WindowOpen result = host_->openWindow({ .id = kWindowId, .size = WindowSize::Large, .resizable = false, .maximizable = false }, pomodoroWindow(view));
    windowOpen_ = result == WindowOpen::Opened || result == WindowOpen::Raised;
    if (windowOpen_ && !wasOpen) {
        question_ = PomodoroQuestion::None;
        undoInWindow_ = false;
    }
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
    host_->startActivity({ .id = kPanelId, .relevance = 0.4f, .content = pomodoroContent(*session_, plan_) });
    host_->wakeAt(tl.playing ? std::optional<Ms>(tl.anchoredAt + (tl.duration - tl.position)) : std::nullopt);
    showWindow();
}

} // namespace cup
