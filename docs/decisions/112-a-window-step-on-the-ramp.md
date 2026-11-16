# 112: Windows get their own step on the type ramp

Status: Accepted (amends 090 and 104: a window's text and room). Core tested; the look on Windows needs a run.

**Context.** Decision 111 made the card scale the default for cards and
widgets. The user asked for standalone windows (Settings, the timer's) to
grow too, but less: their pages are rows read at a desk, not one thing
glanced at. They shared `Title`, `Body` and `Caption` with the island's
cards and lists, so raising those would have moved the island as well.

**Decision.**
- **Three window styles**: `WindowTitle` 18/700, `WindowBody` 16/600,
  `WindowCaption` 14/600 (`window…FontSize`, `window…FontWeight`): one
  pixel over the island's 17/15/13, half a card's step. A window body's
  detail is `WindowCaption` (`detailStyleOf`).
- **The window's layout swaps them in** (`windowStyled` in
  `core/window_layout.cpp`) for the page, the sidebar and the footer, Text
  and TimeLabel alike. Providers keep writing the plain styles, so a row
  reads the same built for a card or a window, and Settings' and the timer's
  windows needed no change.
- **The window-only pieces grow a little**: `PageTitle` 26 at 800 (from 24
  at 700), `Heading` 15 (from 14), grouped rows 11 above and below (from 9),
  groups 16 apart (from 12) with 14 corners (from 12), the sidebar 200 wide
  (from 190) with a 60 header and 40 entries (from 56, 38), a switch 40 × 22
  (from 36 × 20), segments 32 (from 30), key caps 26 (from 24).
- **Home's key goes all the way up**: the page title's line (34) is now
  taller than the back arrow (32) beside it, which put the first stop 1 px
  under the margin and Home scrolled to 1. A stop within one margin of the
  page's top scrolls to 0 (`scrollToShow`).

**Not taken.** Scaling the whole window by a factor: buttons, icons and
the title bar are right as they are, and text would land on fractional
pixels. Bigger row buttons: the compact chevrons and ticks read as the
row's end, not its main control.

**Consequences.** Labels drawn inside Segments, Keys and Tags stay
Caption-sized; their tracks grew, so they get a little more air.
