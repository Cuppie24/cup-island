# 002: No hover reaction

Status: Accepted (revisit with PS §12 Q5)

**Context.** It's tempting to make the pill react when the cursor approaches.

**Decision.** No hover animation. The real Dynamic Island is touch-only. The
island also sits at the top edge, where the cursor passes constantly during
unrelated work, so a proximity pulse would be noise. Attention animations are
event-driven instead (alerts, PS §3.4), so they always mean something new happened.

**Consequences.** `PointerMoved` exists only to keep Expanded open while the
user is reading it. A small "peek" on hover in Compact is still an open
question (PS §12 Q5).
