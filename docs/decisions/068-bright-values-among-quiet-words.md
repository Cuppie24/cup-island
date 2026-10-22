# 068: Bright values among quiet words

Status: Accepted

**Context.** Pomodoro's today line ("2 cycles · 3 h 20 min") was one quiet
caption. The mockup it follows picks the numbers out: bright values, dim
words, one size. A text was one colour, its style's.

**Decision.** `Text::bright`: drawn in `titleTextColor`, the theme's
brightest text, at its own style's size and weight. A line of values and
words is several texts in a hugging row (`Row::hug`, centred as one): the
values bright, the words not. Pomodoro's today line: **2** cycles ·
**3 h 20 min**, all Caption size.

**Consequences.** One flag, no new colour: every theme's brightest text
already reads on its island. Tested in `pomodoro_tests.cpp`.
