# 062: Pomodoro's card doesn't close when left alone

Status: Accepted

**Context.** Expanded closes after `expandedIdleTimeout` without the
pointer moving (TR §5). That suits a glance at media, but the Pomodoro card
is a timer people keep open while they work: it kept closing under them.

**Decision.** `Content::noIdleTimeout`: while its card is shown, Expanded
has no idle timeout (no deadline is armed either). Esc, a click outside the
island, Home and the tabs still close or leave it, as with `keepOpen`.
Pomodoro sets it on every card: dormant, running and its settings page
(decision 061). Home and other cards keep the timeout.

**Consequences.** A scenario in `home.scenario` (`panel <id> awake`,
`start <id> awake`) and checks in `pomodoro_tests.cpp`.
