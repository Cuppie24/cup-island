# 126: The timer window's stop asks with the host's question

Status: Accepted (amends 106: stop asked in place of the controls). Core tested; Windows needs a run.

**Decision.** The window's stop button asks "Stop the session?" with the
host's question over the window (`Dialog::windowId`, decision 122), as
deleting a group does: the window dimmed under its title bar, Cancel and
Stop in a panel, Esc and Enter. Its confirm sends `kStopConfirmed`, the same
as the card's dialog. The header no longer swaps its controls for the
question; `PomodoroQuestion::Stop` is gone. The card's stop and Ctrl+W still
ask with the island's dialog over the card (decisions 020, 034).
