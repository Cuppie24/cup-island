# 128: Reset statistics asks with the host's question too

Status: Accepted (amends 106). Core tested.

**Decision.** In the timer's window, Reset statistics asks with the host's
question over the window (decision 122), as stop does (decision 126). That
was the last question asked in place, so the machinery for it is gone:
`PomodoroQuestion`, the provider's `ask`, `pomodoro_action::kCancel`,
`questionRows` and `confirmActionOf`. On the card, Reset still asks with the
island's dialog (decision 071).
