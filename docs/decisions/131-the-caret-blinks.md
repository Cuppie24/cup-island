# 131: The caret blinks, on the compositor

Status: Accepted (amends 016: "a still caret"). The look on Windows needs a run.

**Context.** Decision 016 kept the caret still: it was painted into the
field's text surface, so blinking it would have repainted that surface twice
a second, and an idle field would have cost a wake-up every half second. The
user asked for a blinking caret in every field, done once by the host.

**Decision.** The caret is no longer painted with the field's text
(`paintInput`). The field painter says where it is (`caretRect`), and every
field's view (`ElementVisuals`, so every TextInput on the island and in
windows: to-dos, search, a text edited in place, "+ Add task") keeps a
hairline sprite there, in `bodyTextColor`, over its text. It blinks by a
looping opacity animation on the compositor (on for half the period, off for
the other, `GetCaretBlinkTime()` each), so an idle field still costs the app
nothing between keys. Each change to the field (a key, a click, a selection)
repaints it, which shows the caret steady and starts the blink over, as
Windows' own caret does. Windows set to not blink (`INFINITE`): it stays on.

**Not taken.** Stopping the blink after a few seconds idle (Windows'
CaretTimeout): the animation costs nothing to keep running.
