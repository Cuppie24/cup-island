# 165: Media's shuffle and repeat sit at the card's edges

Status: Accepted (amends 107). Needs a look on Windows.

**Context.** Decision 107 put shuffle and repeat right beside previous and
next (`Row::edgesBeside`): five controls in one cluster in the middle of the
row, the bottom corners empty. Compared side by side with other layouts, the
cluster read as five equal buttons, though two of them are modes that are
set once and three are the transport used all the time.

**Decision.** Media's controls row no longer sets `edgesBeside`: shuffle is
at the row's start and repeat at its end, one padding in, under the two
times at the ends of the bar's row. Previous, ▶ and next stay centred, the
loose gap apart. With only one of the two offered, it sits alone at its edge
and ▶ is still centred.

**Consequences.** The transport reads as one group and the modes as
another; the card's bottom corners are used. Shuffle and repeat are further
from the pointer when it's on ▶. `Row::edgesBeside` stays for a window
card's name (decision 123).
