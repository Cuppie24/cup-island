# 066: The Pomodoro card after its first run

Status: Accepted (amends 063, 064, 065)

**Context.** The first Windows run of the dial card (065) showed: the ring
shrank on a break ("5:00" is narrower than "25:00"); the gear's hover was
too big for its small grey glyph; ▶'s hover was a rounded square behind a
circle; the timer, the dots, the caption and the controls were packed
together; and the statistics were reachable only through the numbers.

**Decision.**
- **A statistics button** (`Glyph::Chart`, Fluent's AreaChart) beside the
  gear, the same secondary kind. The days in a row and today's line are
  plain quiet text again: no hidden way in.
- **The ring keeps its size:** `Dial::sizedFor`, the plan's longest phase,
  and never under "10:00": a break's 5:00 sits in focus's ring.
- **Secondary hover:** a `secondaryHoverSize` circle centred on the glyph,
  smaller than the 36 px click area, which stays.
- **Primary hover:** no shape behind it; its own fill softens to
  `primaryHoverOpacity` under the pointer. Primary and round buttons get a
  circle's radius, so press feedback and focus follow the circle too.
- **Groups apart:** `Row::apart` puts `groupGap` above a row: the dial, ▶
  and today's line on the dormant card; the dial, the dots with their
  caption, and the controls on the running one. `section` keeps its
  smaller gap for headings on settings pages.

**Consequences.** Sizes and gaps are tested in `pomodoro_tests.cpp`; the
hover looks need a Windows run.
