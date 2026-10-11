#include "providers/pomodoro/pomodoro_provider.h"

#include "core/tokens.h"

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

Panel pomodoroPanel()
{
    const Tokens& t = tokens();
    std::string plan = minutes(t.pomodoroFocus) + " focus, " + minutes(t.pomodoroBreak) + " breaks";
    Content content{
        .label = "Focus",
        .accent = accentOf(Phase::Focus),
        .expandedLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .expandedCenter = { row({ Text{ "Focus", TextStyle::Title } }), row({ Text{ plan } }) },
        .expandedBottom = { row({ Button{ Glyph::Play, act::kStart } }, Align::Center) },
        .tile = { Row{ .items = { Icon{ Glyph::Timer }, Text{ "Focus" } }, .trailing = { Button{ Glyph::Play, act::kStart } } } },
    };
    return { .id = PomodoroProvider::kPanelId, .content = content, .wide = true };
}

// Compact: the countdown centred, the ring filling on the right; no phase
// name (the colour says it). Expanded: a big ring, the phase, and a dot per
// phase showing how many have passed.
Content pomodoroContent(const PomodoroProvider::Session& s)
{
    std::string name = nameOf(s.phase);
    int done = phasesDone(s), total = phaseCount();
    std::string passed = std::to_string(done) + " of " + std::to_string(total) + " phases done";
    if (!s.timeline.playing) passed = "Paused · " + passed;
    TimeLabel left{ .timeline = s.timeline, .countdown = true };
    TimeLabel bigLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Body };
    Progress ring{ .timeline = s.timeline, .shape = ProgressShape::Ring };
    Button pause{ s.timeline.playing ? Glyph::Pause : Glyph::Play, act::kPause };
    Button stop{ Glyph::Stop, act::kStop };
    return {
        .label = name,
        .accent = accentOf(s.phase),
        .compactLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .compactCenter = { row({ bigLeft }, Align::Center) },
        .compactTrailing = { row({ ring }, Align::Center) },
        .minimal = { row({ ring }, Align::Center) },
        .expandedLeading = { row({ ring }, Align::Center) },
        .expandedCenter = { row({ Text{ name, TextStyle::Title } }), row({ Steps{ total, done } }),
                            row({ Text{ passed, TextStyle::Caption } }) },
        .expandedBottom = { row({ TimeLabel{ .timeline = s.timeline }, Progress{ s.timeline }, TimeLabel{ .timeline = s.timeline, .remaining = true } }),
                            Row{ .items = { pause, Button{ Glyph::Next, act::kSkip }, stop }, .align = Align::Center, .loose = true } },
        // Same place on Home as the dormant tile, now live (decision 009).
        .tile = { Row{ .items = { Text{ name }, left }, .trailing = { stop } } },
    };
}

void PomodoroProvider::start(Host& host)
{
    host_ = &host;
    host_->publishPanel(pomodoroPanel());
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

void PomodoroProvider::publish()
{
    const Timeline& tl = session_->timeline;
    host_->startActivity({ .id = kPanelId, .relevance = 0.4f, .content = pomodoroContent(*session_) });
    host_->wakeAt(tl.playing ? std::optional<Ms>(tl.anchoredAt + (tl.duration - tl.position)) : std::nullopt);
}

} // namespace cup
