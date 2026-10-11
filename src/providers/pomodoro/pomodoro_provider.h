#pragma once
// Pomodoro: the first panel (decision 009). Dormant, its Home tile starts a
// session; running, it's an activity that counts down focus rounds and
// breaks. It ends by itself after the long break, or with its stop button.
// Pure: the countdown is host-driven (a Timeline), phase ends come from
// Host::wakeAt, so it builds and tests on any OS.

#include "core/provider.h"

#include <cstddef>
#include <optional>

namespace cup {

namespace pomodoro_action {
inline constexpr const char* kStart = "start";
inline constexpr const char* kPause = "pause"; // toggles pause / resume
inline constexpr const char* kSkip = "skip";   // ends the current phase early, without an alert
inline constexpr const char* kStop = "stop";
} // namespace pomodoro_action

class PomodoroProvider : public Provider {
public:
    const char* id() const override { return "pomodoro"; }
    void start(Host& host) override; // publishes the dormant panel
    void stop() override;            // ends a running session, removes the panel
    void onAction(const Action& action) override;
    void onWake() override;          // the current phase's time is up

    static constexpr const char* kPanelId = "pomodoro.focus"; // the panel and its activity

    enum class Phase { Focus, Break, LongBreak };
    struct Session {
        Phase phase = Phase::Focus;
        std::size_t round = 1; // 1-based focus round
        Timeline timeline;     // the phase: duration, position, playing = not paused
    };

private:
    void begin(Phase phase, std::size_t round); // starts a phase now
    void advance(bool alert);                   // to the next phase, or the end
    void finish();                              // no session any more: dormant panel
    void publish();                             // activity content + the wake-up for the phase end

    Host* host_ = nullptr;
    std::optional<Session> session_;
};

// Content, pure (tested on its own): the dormant panel, and a running session.
Panel pomodoroPanel();
Content pomodoroContent(const PomodoroProvider::Session& session);

} // namespace cup
