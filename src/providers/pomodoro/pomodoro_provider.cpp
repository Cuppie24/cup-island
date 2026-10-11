#include "providers/pomodoro/pomodoro_provider.h"

#include "core/tokens.h"

#include <cmath>
#include <string>

namespace cup {
namespace {

using Phase = PomodoroProvider::Phase;
namespace act = pomodoro_action;

std::string minutes(Ms duration) { return std::to_string(duration.count() / 60000) + " min"; }

const char* nameOf(Phase phase)
{
    switch (phase) {
    case Phase::Focus: return "Focus";
    case Phase::Break: return "Break";
    case Phase::LongBreak: return "Long break";
    }
    return "";
}

Ms durationOf(Phase phase)
{
    const Tokens& t = tokens();
    switch (phase) {
    case Phase::Focus: return t.pomodoroFocus;
    case Phase::Break: return t.pomodoroBreak;
    case Phase::LongBreak: return t.pomodoroLongBreak;
    }
    return t.pomodoroFocus;
}

// Focus and breaks have their own colours (decision 010), in tokens.json.
Color accentOf(Phase phase) { return phase == Phase::Focus ? tokens().pomodoroFocusColor : tokens().pomodoroBreakColor; }

// Every phase of a session, in order: focus, break, ... focus, long break.
int phaseCount() { return 2 * static_cast<int>(tokens().pomodoroRounds); }

int phasesDone(const PomodoroProvider::Session& s)
{
    int round = static_cast<int>(s.round);
    switch (s.phase) {
    case Phase::Focus: return 2 * (round - 1);
    case Phase::Break: return 2 * round - 1;
    case Phase::LongBreak: return phaseCount() - 1;
    }
    return 0;
}

Alert phaseAlert(const std::string& title, const std::string& detail, const std::string& activityId, Color accent)
{
    Content content{ .label = title,
                     .accent = accent,
                     .alertLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
                     .alertCenter = { row({ Text{ title, TextStyle::Title } }), row({ Text{ detail } }) } };
    // Time-sensitive: the user asked to be told (PS §6.1). Coalesced: a skip right after never stacks two.
    return { .activityId = activityId, .level = Level::TimeSensitive, .content = content, .coalesceKey = "pomodoro" };
}

} // namespace

// Dormant: a centred card like the running one, so starting changes as
// little as possible: ▶ where the controls go, the full focus time big, the
// plan, then the to-dos (decision 016). The widget is laid out like the
// running one too: the timer icon where the ring goes, the focus time, the plan.
Panel pomodoroPanel(const TodoList& todos, std::size_t scroll)
{
    const Tokens& t = tokens();
    std::string plan = minutes(t.pomodoroFocus) + " focus, " + minutes(t.pomodoroBreak) + " breaks";
    std::string rounds = "Focus · " + std::to_string(t.pomodoroRounds) + " rounds";
    Content content{
        .label = "Focus",
        .accent = accentOf(Phase::Focus),
        .card = CardLayout::Centred,
        .expandedCenter = { row({ Button{ Glyph::Play, act::kStart } }, Align::Center),
                            row({ Text{ .value = formatDuration(t.pomodoroFocus), .style = TextStyle::Display, .centred = true } }),
                            row({ Text{ .value = plan, .style = TextStyle::Caption, .centred = true } }) },
        .tileLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .tile = { row({ Text{ formatDuration(t.pomodoroFocus), TextStyle::Title } }), row({ Text{ rounds, TextStyle::Caption } }) },
    };
    appendTodoRows(content.expandedCenter, todos, scroll);
    return { .id = PomodoroProvider::kPanelId, .content = content, .wide = true };
}

// Compact: the countdown centred, the ring filling on the right; no phase
// name (the colour says it). Expanded, a centred card (decision 014): the
// controls on top, a big countdown in the middle, a bar for the phase under
// it (as wide as the dots), then a dot per phase and what they mean. The ring is for compact,
// the bubble and the widget, where there's no room for a big countdown.
// The widget (decision 019): the ring filling, the time left, the phase and round.
Content pomodoroContent(const PomodoroProvider::Session& s, const TodoList& todos, std::size_t scroll)
{
    std::string name = nameOf(s.phase);
    int done = phasesDone(s), total = phaseCount();
    std::string caption = name + " · " + std::to_string(done) + " of " + std::to_string(total) + " done";
    if (!s.timeline.playing) caption = "Paused · " + caption;
    std::string glance = s.timeline.playing ? name : "Paused";
    if (s.phase != Phase::LongBreak) glance += " · " + std::to_string(s.round) + "/" + std::to_string(tokens().pomodoroRounds);
    TimeLabel tileLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Title };
    TimeLabel compactLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Body };
    TimeLabel bigLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Display };
    Progress ring{ .timeline = s.timeline, .shape = ProgressShape::Ring };
    Button pause{ s.timeline.playing ? Glyph::Pause : Glyph::Play, act::kPause };
    Button stop{ Glyph::Stop, act::kStop };
    Content content{
        .label = name,
        .accent = accentOf(s.phase),
        .card = CardLayout::Centred,
        .compactLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .compactCenter = { row({ compactLeft }, Align::Center) },
        .compactTrailing = { row({ ring }, Align::Center) },
        .minimal = { row({ ring }, Align::Center) },
        .expandedCenter = {
            Row{ .items = { pause, Button{ Glyph::Next, act::kSkip }, stop }, .align = Align::Center, .loose = true },
            row({ bigLeft }, Align::Center),
            Row{ .items = { Progress{ .timeline = s.timeline } }, .span = RowSpan::MatchBelow }, // as wide as the dots
            row({ Steps{ total, done } }, Align::Center),
            row({ Text{ .value = caption, .style = TextStyle::Caption, .centred = true } }, Align::Center),
        },
        // Same place on Home as the dormant tile, now live (decision 009).
        .tileLeading = { row({ ring }, Align::Center) },
        .tile = { row({ tileLeft }), row({ Text{ glance, TextStyle::Caption } }) },
    };
    appendTodoRows(content.expandedCenter, todos, scroll); // what to work on, under the timer (decision 016)
    return content;
}

void PomodoroProvider::start(Host& host)
{
    host_ = &host;
    if (storage_.load) todos_ = TodoList::load(storage_.load());
    host_->publishPanel(pomodoroPanel(todos_));
}

void PomodoroProvider::stop()
{
    if (!host_) return;
    if (session_) host_->endActivity(kPanelId);
    session_.reset();
    host_->removePanel(kPanelId);
    host_ = nullptr;
}

void PomodoroProvider::onAction(const Action& action)
{
    if (!host_ || action.activityId != kPanelId) return;
    namespace todo = todo_action;
    if (action.actionId == todo::kClearDone || action.actionId == todo::kClearAll) {
        host_->ask(clearQuestion(action.actionId, todos_, kPanelId)); // clears only once confirmed (decision 019)
        return;
    }
    if (action.actionId == todo::kScroll) {
        std::size_t next = clampScroll(static_cast<long long>(todoScroll_) + std::llround(action.value), todos_);
        if (next != todoScroll_) {
            todoScroll_ = next;
            showTodos(/* save */ false);
        }
        return;
    }
    if (todos_.apply(action.actionId, action.text)) {
        // A new to-do scrolls into view (the end); otherwise keep the window, within the list.
        long long want = action.actionId == todo::kAdd ? static_cast<long long>(todos_.items().size()) : static_cast<long long>(todoScroll_);
        todoScroll_ = clampScroll(want, todos_);
        showTodos();
        return;
    }
    Ms now = host_->now();
    if (action.actionId == act::kStart && !session_) {
        begin(Phase::Focus, 1);
    } else if (action.actionId == act::kPause && session_) {
        Timeline& tl = session_->timeline;
        tl.position = tl.positionAt(now);
        tl.anchoredAt = now;
        tl.playing = !tl.playing;
        publish();
    } else if (action.actionId == act::kSkip && session_) {
        advance(/* alert */ false);
    } else if (action.actionId == act::kStop && session_) {
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
    advance(/* alert */ true);
}

void PomodoroProvider::begin(Phase phase, std::size_t round)
{
    Ms duration = durationOf(phase);
    session_ = Session{ phase, round, { .duration = duration, .anchoredAt = host_->now(), .playing = true } };
    publish();
}

// Focus -> break -> focus ... -> long break after the last round -> done.
void PomodoroProvider::advance(bool alert)
{
    const Tokens& t = tokens();
    Session s = *session_;
    if (s.phase == Phase::LongBreak) {
        finish();
        if (alert) host_->postAlert(phaseAlert("Session done", std::to_string(t.pomodoroRounds) + " rounds of focus", "", accentOf(Phase::Break)));
        return;
    }
    if (s.phase == Phase::Focus) {
        Phase next = s.round >= t.pomodoroRounds ? Phase::LongBreak : Phase::Break;
        begin(next, s.round);
        if (alert) host_->postAlert(phaseAlert(next == Phase::LongBreak ? "Long break" : "Break time",
                                               minutes(durationOf(next)), kPanelId, accentOf(next)));
        return;
    }
    begin(Phase::Focus, s.round + 1);
    if (alert) host_->postAlert(phaseAlert("Back to focus", "Round " + std::to_string(s.round + 1) + " of " +
                                                                std::to_string(t.pomodoroRounds), kPanelId, accentOf(Phase::Focus)));
}

void PomodoroProvider::finish()
{
    session_.reset();
    host_->wakeAt(std::nullopt);
    host_->endActivity(kPanelId);
}

void PomodoroProvider::showTodos(bool save)
{
    if (save && storage_.save) storage_.save(todos_.save());
    if (session_) publish();
    else host_->publishPanel(pomodoroPanel(todos_, todoScroll_));
}

void PomodoroProvider::publish()
{
    const Timeline& tl = session_->timeline;
    host_->startActivity({ .id = kPanelId, .relevance = 0.4f, .content = pomodoroContent(*session_, todos_, todoScroll_) });
    host_->wakeAt(tl.playing ? std::optional<Ms>(tl.anchoredAt + (tl.duration - tl.position)) : std::nullopt);
}

} // namespace cup
