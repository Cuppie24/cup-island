# 176: The search card's field covers the glow under it

Status: Accepted; built, not yet run on Windows (amends 079, 166)

**Context.** An open card has a glow along its bottom edge while a cover's
colours are on (decision 166). The Search card is little more than its
field (decision 079), so the glow lies right behind the field, and the
field's box is see-through (`inputColor`, a tenth white): the glow showed
through it and tinted it. The user asked for the field to be above the
glow, and of three readings (a solid field; the field moved up, clear of
the glow; no glow on Search) picked the first.

**Decision.** On the island, a search card's field (`TextInput::large`:
`solidBacked`, core) has the island's colour, solid, under its box: the
same shape, in the same visual, so it shows and fades exactly with the box
and recolours with the island. The glow stops at the field's edge.
Other fields, and fields in providers' windows, are as they were.

**Consequences.** On glass, the search field is no longer see-through: it's
the island's colour with the box's tint over it, a solid bar on a glass
card. The backing is one colour (`islandColor`), so on a card filled with a
cover's two-colour gradient it matches the left end exactly and the right
end nearly. Tested: `solidBacked` (`tests/content_tests.cpp`); the look
needs a Windows run.
