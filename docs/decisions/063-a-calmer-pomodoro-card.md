# 063: A calmer Pomodoro card

Status: Accepted; the plan moved to the settings page by 064

**Context.** With statistics (061) the dormant card had seven buttons (a
chart, the gear, four − / +, ▶) and three lines of text of equal weight
around the big time. Too much going on; the time no longer led.

**Decision.**
- **The plan's − and + show on hover** (`Button::revealOnHover`, as a
  to-do's delete): the row reads "Focus      −  25 min  +", − left of the
  length, + right of it. Hidden, they still take their room, and the
  length keeps a column as wide as the longest it can be
  (`Text::sizedAs`, "120 min"), so the two rows line up, buttons or not.
  The plan stays on the card (044): it's what you change before starting.
- **A row's buttons show anywhere along it** (`rowAt`): between its items
  too, not only over one, so moving along the row doesn't make them blink.
- **No chart button.** The numbers are the way in: the days in a row in
  the top-left corner and today's line both open the statistics page.
  They're quiet label buttons (`Button::quiet`: Caption style and colour).
- **Today's line** is "2 cycles · 3 h 20 min": "today" and "Focus time"
  said what the place already says.
- **Three levels of text, already:** Caption is the theme's dimmest text
  colour, Body dimmer than Title. Rather than a new grey, secondary text
  takes the small style: the plan's names, today's line, the corner.
- **Groups:** ▶ and today's line each start a section (`Row::section`,
  `sectionGap`): timer and plan, start, today.
- **The top row hugs** (`Row::hug`): the big time is as wide as it is, so
  it centres on the card, not in the room the corners leave (the corner's
  count had pushed it right).

**Consequences.** Tested in `pomodoro_tests.cpp` (the rows' columns, the
hover band, the groups) and `pomodoro_stats_tests.cpp` (the time centred
beside "🔥 12 days"). How it looks needs a Windows run.
